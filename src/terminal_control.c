#include "terminal_control.h"

#include <termios.h>
#include <unistd.h>

void ghostcode_disable_flow_control(void) {
    struct termios settings;

    if (tcgetattr(STDIN_FILENO, &settings) != 0) {
        return;
    }

    settings.c_iflag &= ~(IXON);
    tcsetattr(STDIN_FILENO, TCSANOW, &settings);
}
