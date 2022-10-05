//get dir
using namespace std;
#include <fstream>
char dlldir[320];
char* GetDirectoryFile(char* filename)
{
    static char path[320];
    strcpy_s(path, dlldir);
    strcat_s(path, filename);
    return path;
}

//log
void Log(const char* fmt, ...)
{
    if (!fmt)	return;

    char		text[4096];
    va_list		ap;
    va_start(ap, fmt);
    vsprintf_s(text, fmt, ap);
    va_end(ap);

    ofstream logfile(GetDirectoryFile((PCHAR)"log.txt"), ios::app);
    if (logfile.is_open() && text)	logfile << text << endl;
    logfile.close();
}

HWND tmp_window = NULL;
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
    DWORD wndProcId;
    GetWindowThreadProcessId(handle, &wndProcId);

    if (GetCurrentProcessId() != wndProcId)
        return TRUE; // skip to next window

    tmp_window = handle;
    return FALSE; // window found abort search
}

HWND GetProcessWindow()
{
    tmp_window = NULL;
    EnumWindows(EnumWindowsCallback, NULL);
    return tmp_window;
}


class BotCoordinate {
public:
    long x;
    long y;
    BotCoordinate(long x, long y) {
        this->x = x;
        this->y = y;
    }
    BotCoordinate diff(BotCoordinate coord2) {
        return BotCoordinate(x - coord2.x, y - coord2.y);
    }
    bool equals(BotCoordinate coord2) {
        return x == coord2.x && y == coord2.y;
    }
    bool lessThan(long i) {
        return abs(x) < i && abs(y) < i;
    }


};

// This class needs to be 3 bytes long. Do not add virtual functions since this will add a vtable pointer to the struct
class Rgb {

public:
    BYTE r;
    BYTE g;
    BYTE b;
    bool rgbInRange(Rgb compare, long range) {
        return abs((long)r - (long)compare.r) <= range &&
            abs((long)g - (long)compare.g) <= range &&
            abs((long)b - (long)compare.b) <= range;

    }
    bool rgbEqualPlayer() {
        Rgb player = { 255, 0, 0 };//red
        return rgbInRange(player, 50);
    }
    char* format(char* buffer) {
        //sprintf(buffer, "Rgb(%ld,%ld,%ld)", r, g, b);
        return buffer;
    }
};

char* renderedTexture = NULL;
BotCoordinate closestEnemyPixelToCrosshair = BotCoordinate(-1, -1);

#define isSnipingKeyDown() (GetAsyncKeyState(VK_SHIFT) & 0x8000)
#define isShootingKeyDown() (GetAsyncKeyState(VK_LBUTTON) & 0x8000)

#define AIM_FIELD_OF_VIEW 100//200

BotCoordinate screenCenter() {
    return BotCoordinate(gameWidth / 2, gameHeight / 2);
}

/*
  //you can also put this in a thread
        //for (auto start = std::chrono::steady_clock::now(), now = start; now < start + std::chrono::seconds{ 10 }; now = std::chrono::steady_clock::now())
        //{
            //do something
        //}
        //printf("Did something for 10 seconds.\n");

*/

void click() {
    INPUT input;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    input.type = INPUT_MOUSE;
    input.mi.dy = 0;
    input.mi.dx = 0;
    input.mi.mouseData = 0;
    input.mi.dwExtraInfo = 0;
    input.mi.time = 0;
    SendInput(1, &input, sizeof(input));
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(input));
}

DWORD WINAPI reverseSnipingRecoil(LPVOID) {
    Sleep(500);
    INPUT input;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.type = INPUT_MOUSE;
    input.mi.dy = 260;
    input.mi.dx = 0;
    input.mi.mouseData = 0;
    input.mi.dwExtraInfo = 0;
    input.mi.time = 0;

    SendInput(1, &input, sizeof(input));
    return 0;
}

void applyAimBotMouseMovement(BotCoordinate shotCoordinate)
{
    BotCoordinate diff = shotCoordinate.diff(screenCenter());
    INPUT input;
    double factor = 0.5; //0.5;
    double maximum = 8000.0; //8.0;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.type = INPUT_MOUSE;

    if (abs(diff.x) < 3 && abs(diff.y < 3)) {
        factor = 1.0;
    }

    if (isSnipingKeyDown()) {
        factor = 1.0;
    }

    input.mi.dy = (long)(min((double)diff.y * factor, maximum));
    input.mi.dx = (long)(min((double)diff.x * factor, maximum));
    input.mi.mouseData = 0;
    input.mi.dwExtraInfo = 0;
    input.mi.time = 0;

    SendInput(1, &input, sizeof(input));

    if (isSnipingKeyDown() && diff.lessThan(3)) {//2
        click();
    }

}

Rgb pixels[10000000];
void readPixelsAndShoot() {
    closestEnemyPixelToCrosshair = BotCoordinate(10, 10);

    if (!isSnipingKeyDown() || GetForegroundWindow() != hwndWindow)
        //if ((!isShootingKeyDown() && !isSnipingKeyDown())||GetForegroundWindow() != hwndWindow) 
    {
        return;
    }

    BotCoordinate center = screenCenter();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(center.x - AIM_FIELD_OF_VIEW, center.y - AIM_FIELD_OF_VIEW, AIM_FIELD_OF_VIEW * 2, AIM_FIELD_OF_VIEW * 2, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // optimised by reading out from pixels at the center of the screen first. 
    // bottomLeftCorner begins in the center of the screen and moves to the bottom left corner of the sticky aim square. 
    // When a pixel is found matching the enemy colour, apply aimbot mouse movement
    long bottomLeftCorner = AIM_FIELD_OF_VIEW - 1;
    long width = 2;
    while (bottomLeftCorner >= 0) {
        for (long y = 0; y < width; y++) {
            for (long x = 0; x < width; x++) {

                long ycoord = (bottomLeftCorner + y);
                long xcoord = bottomLeftCorner + x;

                Rgb p = pixels[xcoord + ycoord * (AIM_FIELD_OF_VIEW + AIM_FIELD_OF_VIEW)];

                if (p.rgbEqualPlayer()) {
                    // y goes the other way when converting glReadPixel coordinates to screen coordinates
                    closestEnemyPixelToCrosshair = BotCoordinate((gameWidth / 2 - AIM_FIELD_OF_VIEW) + xcoord, (gameHeight / 2 + AIM_FIELD_OF_VIEW) - ycoord);
                    applyAimBotMouseMovement(closestEnemyPixelToCrosshair);
                    return;
                }
                // skip pixels we have already tested since we read inner pixels before the outer pixels
                if (y != 0 && y != width - 1 && x == 0) {
                    x = width - 2;
                }
            }
        }
        bottomLeftCorner--;
        width += 2;
    }
    closestEnemyPixelToCrosshair = BotCoordinate(10, 10);
}

void drawAimbotSquare() {
    //drawSquare(screenCenter(), AIM_FIELD_OF_VIEW + 3);
}

void drawAimAtDisplay() {
    if (closestEnemyPixelToCrosshair.x == -1) return;
    //drawSquare(closestEnemyPixelToCrosshair, 5);
}

