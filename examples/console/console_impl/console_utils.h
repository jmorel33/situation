#ifndef CONSOLE_UTILS_H
#define CONSOLE_UTILS_H

static void ConsoleCopyString(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_size, "%s", src);
}

static bool ParseCSIResponse(const char* response, size_t length) {
    if (length > 3 && response[0] == '\x1B' && response[1] == '[') {
        int i = 2;
        int row = 0, col = 0;

        while (i < (int)length && response[i] >= '0' && response[i] <= '9') {
            row = row * 10 + (response[i] - '0');
            i++;
        }

        if (i < (int)length && response[i] == ';') {
            i++;
            while (i < (int)length && response[i] >= '0' && response[i] <= '9') {
                col = col * 10 + (response[i] - '0');
                i++;
            }

            if (i < (int)length && response[i] == 'R') {
                if (cursor_tracker.waiting_for_position) {
                    cursor_tracker.row = row;
                    cursor_tracker.col = col;
                    cursor_tracker.position_received = true;
                    cursor_tracker.waiting_for_position = false;
                    return true;
                }
            }
        }
    }
    return false;
}

#endif /* CONSOLE_UTILS_H */
