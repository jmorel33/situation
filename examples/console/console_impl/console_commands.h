#ifndef CONSOLE_COMMANDS_H
#define CONSOLE_COMMANDS_H

static void ProcessCommand(const char* command) {
    char* tokens[MAX_TOKENS];
    char* buffer_to_free = NULL;
    int token_count = TokenizeCommand(command, tokens, &buffer_to_free);

    if (token_count == 0) {
        console.prompt_pending = true;
        if (buffer_to_free) free(buffer_to_free);
        return;
    }
    const char* cmd = tokens[0];

    if (strcmp(cmd, "cls") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'cls' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\x1B[3J\x1B[2J\x1B[H");
        }
    } else if (strcmp(cmd, "clear") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'clear' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\x1B[2J\x1B[H");
        }
    } else if (strcmp(cmd, "echo") == 0) {
        if (token_count == 1) {
            KTerm_WriteString(term, "\n");
        } else {
            for (int i = 1; i < token_count; i++) {
                KTerm_WriteString(term, tokens[i]);
                if (i < token_count - 1) KTerm_WriteChar(term, ' ');
            }
            KTerm_WriteString(term, "\n");
        }
    } else if (strcmp(cmd, "noecho") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'noecho' takes no arguments\x1B[0m\n");
        } else {
            console.echo_enabled = false;
            KTerm_WriteString(term, "\x1B[?12l");
            KTerm_WriteString(term, "Echo disabled\n");
        }
    } else if (strcmp(cmd, "echo_on") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'echo_on' takes no arguments\x1B[0m\n");
        } else {
            console.echo_enabled = true;
            KTerm_WriteString(term, "\x1B[?12h");
            KTerm_WriteString(term, "Echo enabled\n");
        }
    } else if (strcmp(cmd, "password") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'password' takes no arguments\x1B[0m\n");
        } else {
            console.password_mode = true;
            KTerm_WriteString(term, "Password mode enabled (input will show as *)\n");
        }
    } else if (strcmp(cmd, "normal") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'normal' takes no arguments\x1B[0m\n");
        } else {
            console.password_mode = false;
            KTerm_WriteString(term, "Normal input mode\n");
        }
    } else if (strcmp(cmd, "test") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'test' takes no arguments\x1B[0m\n");
        } else {
            const char* test_seq = "\x1B[31mRed \x1B[32mGreen \x1B[33mYellow \x1B[34mBlue \x1B[35mMagenta \x1B[36mCyan \x1B[37mWhite\x1B[0m\n";
            KTerm_WriteString(term, test_seq);
        }
    } else if (strcmp(cmd, "color_test") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'color_test' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "Standard Colors:\n");
            for (int i = 0; i < 8; i++) KTerm_WriteFormat(term, "\x1B[%dm███ ", 30 + i);
            KTerm_WriteString(term, "\x1B[0m\nBright Colors:\n");
            for (int i = 0; i < 8; i++) KTerm_WriteFormat(term, "\x1B[%dm███ ", 90 + i);
            KTerm_WriteString(term, "\x1B[0m\n\n256-color palette (first 32):\n");
            for (int i = 0; i < 32; i++) {
                KTerm_WriteFormat(term, "\x1B[38;5;%dm█", i);
                if ((i + 1) % 16 == 0) KTerm_WriteString(term, "\n");
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        }
    } else if (strcmp(cmd, "rainbow") == 0) {
        if (token_count == 1) {
            const char* text = "Rainbow colors using true color support!";
            int len = strlen(text);
            for (int i = 0; i < len; i++) {
                int r = (int)(127 * (1 + sin(i * 0.3)));
                int g = (int)(127 * (1 + sin(i * 0.3 + 2)));
                int b = (int)(127 * (1 + sin(i * 0.3 + 4)));
                KTerm_WriteFormat(term, "\x1B[38;2;%d;%d;%dm%c", r, g, b, text[i]);
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        } else {
            int char_idx_overall = 0;
            for (int i = 1; i < token_count; i++) {
                const char* text_segment = tokens[i];
                int len = strlen(text_segment);
                for (int j = 0; j < len; j++) {
                    int r = (int)(127 * (1 + sin(char_idx_overall * 0.3)));
                    int g = (int)(127 * (1 + sin(char_idx_overall * 0.3 + 2.094395)));
                    int b = (int)(127 * (1 + sin(char_idx_overall * 0.3 + 4.188790)));
                    KTerm_WriteFormat(term, "\x1B[38;2;%d;%d;%dm%c", r, g, b, text_segment[j]);
                    char_idx_overall++;
                }
                if (i < token_count - 1) {
                    KTerm_WriteChar(term, ' ');
                    char_idx_overall++;
                }
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        }
    } else if (strcmp(cmd, "cursor_test") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'cursor_test' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Cursor movement test:\nMoving cursor around...\n");
            KTerm_WriteString(term, "\x1B[10;10H*\x1B[12;15H*\x1B[8;20H*\x1B[15;5H*\x1B[H");
        }
    } else if (strcmp(cmd, "scroll_test") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'scroll_test' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Scroll test - generating many lines:\n");
            for (int i = 1; i <= 60; i++) KTerm_WriteFormat(term, "Line %d - This is a scrolling test\n", i);
        }
    } else if (strcmp(cmd, "performance") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'performance' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Performance test - sending large amount of data:\n");
            for (int i = 0; i < 1000; i++) KTerm_WriteFormat(term, "Performance test line %d with some text content\n", i);
        }
    } else if (strcmp(cmd, "demo") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'demo' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "\x1B[2J\x1B[H");
            KTerm_WriteString(term, "   \x1B[1;37mK-Term Capability Showcase\x1B[0m\n\n");
            KTerm_WriteString(term, "   \x1B[1mBold\x1B[0m  \x1B[2mDim\x1B[0m  \x1B[3mItalic\x1B[0m  \x1B[4mUnderline\x1B[0m  \x1B[7mInverse\x1B[0m\n\n");
            KTerm_WriteString(term, "   \x1B[90mType 'color_test', 'graphics', or 'rainbow <text>' for more.\x1B[0m\n\n");
        }
    } else if (strcmp(cmd, "graphics") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'graphics' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "\n\x1B[1;33m   CP437 Character Set Showcase:\x1B[0m\n\n");
            for (int row = 8; row < 16; row++) {
                KTerm_WriteFormat(term, "   \x1B[90m%X0:\x1B[0m ", row);
                for (int col = 0; col < 16; col++) {
                    unsigned char ch = (unsigned char)(row * 16 + col);
                    KTerm_WriteFormat(term, " \x1B[36m%c\x1B[0m ", ch);
                }
                KTerm_WriteString(term, "\n");
            }
            KTerm_WriteString(term, "\n");
        }
    } else if (strcmp(cmd, "blink") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'blink' takes no arguments\x1B[0m\n");
        else KTerm_WriteString(term, "This text should \x1B[5mblink\x1B[0m if blinking is supported.\n");
    } else if (strcmp(cmd, "history") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'history' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Command history:\n");
            for (int i = 0; i < console.history_count; i++) KTerm_WriteFormat(term, "%2d: %s\n", i + 1, console.command_history[i]);
        }
    } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'exit/quit' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "Goodbye!\n");
            should_exit = true;
        }
    } else if (strcmp(cmd, "pipeline_stats") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'pipeline_stats' takes no arguments\x1B[0m\n");
        else KTerm_ShowDiagnostics(term);
    } else if (strcmp(cmd, "set_fps") == 0) {
        if (token_count != 2) KTerm_WriteString(term, "\x1B[31mError: 'set_fps' requires one argument (FPS value)\x1B[0m\n");
        else {
            int fps = atoi(tokens[1]);
            if (fps > 0 && fps <= 120) {
                KTerm_SetPipelineTargetFPS(term, fps);
                KTerm_WriteFormat(term, "Target FPS set to %d\n", fps);
            } else KTerm_WriteString(term, "Invalid FPS value (1-120)\n");
        }
    } else if (strcmp(cmd, "set_budget") == 0) {
        if (token_count != 2) KTerm_WriteString(term, "\x1B[31mError: 'set_budget' requires one argument (percentage 0.0-1.0)\x1B[0m\n");
        else {
            double pct = atof(tokens[1]);
            if (pct > 0.0 && pct <= 1.0) {
                KTerm_SetPipelineTimeBudget(term, pct);
                KTerm_WriteFormat(term, "Pipeline time budget set to %.1f%%\n", pct * 100.0);
            } else KTerm_WriteString(term, "Invalid budget percentage (0.01-1.0)\n");
        }
    } else if (strcmp(cmd, "term_status") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_status' takes no arguments\x1B[0m\n");
        } else {
            KTermStatus status = KTerm_GetStatus(term);
            KTerm_WriteString(term, "\n--- KTerm Library Status ---\n");
            KTerm_WriteFormat(term, "Input Pipeline Usage: %zu bytes\n", status.pipeline_usage);
            KTerm_WriteFormat(term, "Keyboard Event Usage: %zu events\n", status.key_usage);
            KTerm_WriteFormat(term, "Input Pipeline Overflowed: %s\n", status.overflow_detected ? "YES" : "NO");
            KTerm_WriteFormat(term, "Avg Char Process Time: %.6f ms\n", status.avg_process_time * 1000.0);
            KTerm_WriteString(term, "-----------------------------\n");
        }
    } else if (strcmp(cmd, "term_vtlevel") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_vtlevel' takes no arguments\x1B[0m\n");
        } else {
            VTLevel level = KTerm_GetLevel(term);
            KTerm_WriteFormat(term, "\nCurrent KTerm VT Level: %d (", level);
            switch (level) {
                case VT_LEVEL_52:   KTerm_WriteString(term, "VT52"); break;
                case VT_LEVEL_100:  KTerm_WriteString(term, "VT100"); break;
                case VT_LEVEL_220:  KTerm_WriteString(term, "VT220"); break;
                case VT_LEVEL_320:  KTerm_WriteString(term, "VT320"); break;
                case VT_LEVEL_420:  KTerm_WriteString(term, "VT420"); break;
                case VT_LEVEL_510:  KTerm_WriteString(term, "VT510"); break;
                case VT_LEVEL_520:  KTerm_WriteString(term, "VT520"); break;
                case VT_LEVEL_525:  KTerm_WriteString(term, "VT525"); break;
                case VT_LEVEL_K95:  KTerm_WriteString(term, "K95"); break;
                case VT_LEVEL_XTERM: KTerm_WriteString(term, "XTERM"); break;
                case VT_LEVEL_TT:    KTerm_WriteString(term, "Tera Term"); break;
                case VT_LEVEL_PUTTY: KTerm_WriteString(term, "PuTTY"); break;
                case VT_LEVEL_ANSI_SYS: KTerm_WriteString(term, "ANSI.SYS"); break;
                default: KTerm_WriteString(term, "Unknown"); break;
            }
            KTerm_WriteString(term, ")\n");
        }
    } else if (strcmp(cmd, "term_da") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_da' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\nRequesting Primary DA (ESC[c)...\n");
            KTerm_WriteString(term, "\x1B[c");
            KTerm_WriteString(term, "Requesting Secondary DA (ESC[>c)...\n");
            KTerm_WriteString(term, "\x1B[>c");
        }
    } else if (strcmp(cmd, "term_runtest") == 0) {
        if (token_count != 2) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_runtest' requires one argument (e.g., cursor, colors, all)\x1B[0m\n");
        } else {
            KTerm_WriteFormat(term, "\nRequesting terminal to run test: %s\n", tokens[1]);
            KTerm_RunTest(term, tokens[1]);
        }
    } else if (strcmp(cmd, "term_showinfo") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_showinfo' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\nRequesting terminal to show its info:\n");
            KTerm_ShowInfo(term);
        }
    } else if (strcmp(cmd, "term_diagbuffers") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_diagbuffers' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\nRequesting terminal to show buffer diagnostics:\n");
            KTerm_ShowDiagnostics(term);
        }
    } else if (strcmp(cmd, "sys_info") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- System Device Information ---\x1B[0m\n");
        SitHelperPrintDeviceInfo();
    } else if (strcmp(cmd, "sys_displays") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- Physical Display Information ---\x1B[0m\n");
        int display_count = 0;
        SituationDisplayInfo* displays = NULL;
        SituationGetDisplays(&displays, &display_count);
        if (displays) {
            SitHelperPrintDisplayInfo(displays, display_count);
            SituationFreeDisplays(displays, display_count);
            KTerm_WriteFormat(term, "  Current Situation Mon Index: %d\n", SituationGetCurrentMonitor());
        } else {
            char* err_msg = NULL;
            SituationGetLastErrorMsg(&err_msg);
            KTerm_WriteFormat(term, "\x1B[31mError getting display info: %s\x1B[0m\n", err_msg ? err_msg : "Unknown");
            if (err_msg) free(err_msg);
        }
    } else if (strcmp(cmd, "sys_audio") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- Audio Playback Device Information ---\x1B[0m\n");
        int audio_device_count = 0;
        SituationAudioDeviceInfo* audio_devices = SituationEnumerateAudioDevices(&audio_device_count);
        if (audio_devices) {
            SitHelperPrintAudioDeviceInfo(audio_devices, audio_device_count);
            SituationFreeDeviceList(audio_devices, audio_device_count);
        } else {
            char* err_msg = NULL;
            SituationGetLastErrorMsg(&err_msg);
            KTerm_WriteFormat(term, "\x1B[31mError getting audio devices: %s\x1B[0m\n", err_msg ? err_msg : "No devices or error");
            if (err_msg) free(err_msg);
        }
    } else if (strcmp(cmd, "sys_userdir") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- User Directory ---\x1B[0m\n");
        char* user_dir = SituationGetUserDirectory();
        if (user_dir) {
            KTerm_WriteFormat(term, "  User Profile Directory: %s\n", user_dir);
            free(user_dir);
        } else {
            char* err_msg = NULL;
            SituationGetLastErrorMsg(&err_msg);
            KTerm_WriteFormat(term, "\x1B[31mError getting user directory: %s\x1B[0m\n", err_msg ? err_msg : "Unknown");
            if (err_msg) free(err_msg);
        }
    } else if (strcmp(cmd, "type") == 0) {
        if (token_count < 2) {
            KTerm_WriteString(term, "\x1B[31mUsage: type <filepath>\x1B[0m\n");
        } else {
            char filepath[MAX_COMMAND_BUFFER];
            filepath[0] = '\0';
            for (int i = 1; i < token_count; i++) {
                if (i > 1) strcat(filepath, " ");
                strcat(filepath, tokens[i]);
            }
            FILE* f = fopen(filepath, "rb");
            if (!f) {
                KTerm_WriteFormat(term, "\x1B[31mError: Cannot open '%s'\x1B[0m\n", filepath);
            } else {
                fseek(f, 0, SEEK_END);
                long file_size = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (file_size <= 0) {
                    KTerm_WriteString(term, "\x1B[90m(empty file)\x1B[0m\n");
                } else {
                    char chunk[4096];
                    size_t bytes_read;
                    while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
                        KTerm_PushInput(term, chunk, bytes_read);
                    }
                }
                fclose(f);
            }
        }
    } else if (strcmp(cmd, "pwd") == 0) {
        char cwd[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
            KTerm_WriteFormat(term, "%s\n", cwd);
        } else {
            KTerm_WriteString(term, "\x1B[31mError: Could not get current directory\x1B[0m\n");
        }
    } else if (strcmp(cmd, "cd") == 0) {
        if (token_count < 2) {
            char cwd[MAX_PATH];
            if (GetCurrentDirectoryA(MAX_PATH, cwd))
                KTerm_WriteFormat(term, "%s\n", cwd);
        } else {
            char path[MAX_PATH];
            path[0] = '\0';
            for (int i = 1; i < token_count; i++) {
                if (i > 1) strcat(path, " ");
                strcat(path, tokens[i]);
            }
            if (!SetCurrentDirectoryA(path)) {
                KTerm_WriteFormat(term, "\x1B[31mcd: no such directory: %s\x1B[0m\n", path);
            }
        }
    } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
        const char* target = (token_count > 1) ? tokens[1] : ".";
        char search_path[MAX_PATH];
        snprintf(search_path, sizeof(search_path), "%s\\*", target);
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search_path, &fd);
        if (hFind == INVALID_HANDLE_VALUE) {
            KTerm_WriteFormat(term, "\x1B[31mls: cannot access '%s'\x1B[0m\n", target);
        } else {
            do {
                if (strcmp(fd.cFileName, ".") == 0) continue;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    KTerm_WriteFormat(term, "\x1B[1;34m%s/\x1B[0m\n", fd.cFileName);
                } else {
                    LARGE_INTEGER filesize;
                    filesize.LowPart = fd.nFileSizeLow;
                    filesize.HighPart = fd.nFileSizeHigh;
                    if (filesize.QuadPart < 1024)
                        KTerm_WriteFormat(term, "  %s  (%lld B)\n", fd.cFileName, filesize.QuadPart);
                    else if (filesize.QuadPart < 1024*1024)
                        KTerm_WriteFormat(term, "  %s  (%.1f KB)\n", fd.cFileName, filesize.QuadPart / 1024.0);
                    else
                        KTerm_WriteFormat(term, "  %s  (%.1f MB)\n", fd.cFileName, filesize.QuadPart / (1024.0*1024.0));
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    } else if (strcmp(cmd, "sysinfo") == 0) {
        SituationCPUInfo cpu;
        SituationGPUInfo gpu;
        SituationMemoryInfo mem;
        SituationGetCPUInfo(&cpu);
        SituationGetGPUInfo(&gpu);
        SituationGetMemoryInfo(&mem);
        SituationOSInfo os = SituationGetOSInfo();
        const char* state_str = "READY";
        switch (SituationGetInitState()) {
            case SITUATION_STATE_UNINITIALIZED: state_str = "UNINITIALIZED"; break;
            case SITUATION_STATE_INITIALIZING:  state_str = "INITIALIZING"; break;
            case SITUATION_STATE_READY:         state_str = "READY"; break;
            case SITUATION_STATE_SHUTTING_DOWN: state_str = "SHUTTING DOWN"; break;
        }
        KTerm_WriteString(term, "\n   \x1B[36m");
        KTerm_WriteChar(term, 0xC9);
        for (int i = 0; i < 64; i++) KTerm_WriteChar(term, 0xCD);
        KTerm_WriteChar(term, 0xBB);
        KTerm_WriteString(term, "\x1B[0m\n");
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m \x1B[1;37mKaOS System Information\x1B[0m                                        \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");
        char line[128];
        snprintf(line, sizeof(line), " Situation : v%-12s  KTerm : v%-12s  [%s]",
            SituationGetVersionString(), KTERM_VERSION_STRING, state_str);
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");
        snprintf(line, sizeof(line), " OS        : %s (%s)", os.name, os.version);
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");
        snprintf(line, sizeof(line), " CPU       : %s (%u cores @ %.2f GHz)",
            cpu.name, cpu.thread_count, cpu.clock_speed_ghz);
        line[64] = '\0';
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");
        snprintf(line, sizeof(line), " RAM       : %.1f GB free / %.1f GB total",
            mem.available_bytes / (1024.0*1024*1024),
            mem.total_bytes / (1024.0*1024*1024));
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xC8);
        for (int i = 0; i < 64; i++) KTerm_WriteChar(term, 0xCD);
        KTerm_WriteChar(term, 0xBC);
        KTerm_WriteString(term, "\x1B[0m\n\n");
    } else if (strcmp(cmd, "ps") == 0 || strcmp(cmd, "processes") == 0) {
        int count = 0;
        SituationProcessInfo* procs = SituationGetProcessList(&count);
        if (!procs || count == 0) {
            KTerm_WriteString(term, "\x1B[31mError: Could not enumerate processes\x1B[0m\n");
        } else {
            KTerm_WriteFormat(term, "\x1B[1;33m  %-8s %-10s %s\x1B[0m\n", "PID", "Memory", "Name");
            for (int i = 0; i < count; i++) {
                if (procs[i].memory_bytes == 0 && procs[i].name[0] == '\0') continue;
                if (procs[i].memory_bytes < 1024*1024)
                    KTerm_WriteFormat(term, "  %-8u %6.0f KB  %s\n",
                        procs[i].pid, procs[i].memory_bytes / 1024.0, procs[i].name);
                else
                    KTerm_WriteFormat(term, "  %-8u %6.1f MB  %s\n",
                        procs[i].pid, procs[i].memory_bytes / (1024.0*1024), procs[i].name);
            }
            KTerm_WriteFormat(term, "\n  \x1B[90m(%d processes)\x1B[0m\n", count);
            SituationFreeProcessList(procs, count);
        }
    } else if (strcmp(cmd, "threads") == 0 || strcmp(cmd, "workers") == 0) {
#ifdef SITUATION_ENABLE_THREADING
        SituationThreadPool* pool = SituationGetInternalThreadPool();
        if (!pool) {
            KTerm_WriteString(term, "\x1B[31mThread pool not active\x1B[0m\n");
        } else {
            SituationThreadPoolSnapshot snap;
            SituationGetThreadPoolSnapshot(pool, &snap);
            KTerm_WriteString(term, "\n\x1B[1;36m--- Situation Thread Pool ---\x1B[0m\n");
            KTerm_WriteFormat(term, "  State: %s  |  Workers: %zu  |  Active Jobs: %d\n",
                snap.pool_active ? "\x1B[32mACTIVE\x1B[0m" : "\x1B[31mINACTIVE\x1B[0m",
                snap.worker_count, snap.active_jobs);
        }
#else
        KTerm_WriteString(term, "\x1B[31mThreading not enabled in this build\x1B[0m\n");
#endif
    } else if (strcmp(cmd, "font") == 0) {
        if (token_count < 2) {
            KTerm_WriteString(term, "\x1B[1;33mAvailable fonts:\x1B[0m\n");
            KTerm_WriteString(term, "  \x1B[36mVT220\x1B[0m  \x1B[36mIBM\x1B[0m  \x1B[36mVGA\x1B[0m  \x1B[36mULTIMATE\x1B[0m  \x1B[36mNEC\x1B[0m\n");
            KTerm_WriteString(term, "\nUsage: \x1B[33mfont <name>\x1B[0m\n");
        } else {
            KTerm_SetFont(term, tokens[1]);
            KTerm_WriteFormat(term, "Font set to: %s\n", tokens[1]);
        }
    } else if (strcmp(cmd, "edit") == 0) {
        if (token_count < 2) {
            KTerm_WriteString(term, "\x1B[31mUsage: edit <file>   or   edit --new <file>\x1B[0m\n");
        } else if (strcmp(tokens[1], "--new") == 0) {
            if (token_count < 3) {
                KTerm_WriteString(term, "\x1B[31mError: missing filename for 'edit --new'\x1B[0m\n");
            } else {
                EditorEnter(tokens[2], true);
                if (buffer_to_free) free(buffer_to_free);
                return;
            }
        } else {
            EditorEnter(tokens[1], false);
            if (buffer_to_free) free(buffer_to_free);
            return;
        }
    } else if (strcmp(cmd, "vt_styles") == 0 || strcmp(cmd, "vt_demo") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: command takes no arguments\x1B[0m\n");
        } else {
            ConsoleVtDemoStyles(term);
        }
    } else if (strcmp(cmd, "vt_box") == 0) {
        ConsoleVtRunBoxCommand(tokens, token_count);
    } else if (strcmp(cmd, "vt_menu") == 0) {
        ConsoleVtRunMenuCommand(tokens, token_count);
        if (buffer_to_free) free(buffer_to_free);
        return;
    } else if (strcmp(cmd, "vt_combo") == 0) {
        ConsoleVtRunComboCommand(tokens, token_count);
        if (buffer_to_free) free(buffer_to_free);
        return;
    } else if (strcmp(cmd, "vt_dialog") == 0) {
        ConsoleVtRunDialogCommand(tokens, token_count);
        if (buffer_to_free) free(buffer_to_free);
        return;
    } else if (strcmp(cmd, "vt_styles_capture") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'vt_styles_capture' takes no arguments\x1B[0m\n");
        } else {
            ConsoleVtRunStylesCaptureCommand();
        }
    } else if (strcmp(cmd, "shell") == 0) {
        if (shell_mode) {
            KTerm_WriteString(term, "\x1B[33mAlready in shell mode.\x1B[0m\n");
        } else {
            const char* shell_cmd = (token_count > 1) ? tokens[1] : NULL;
            KTerm_WriteString(term, "\x1B[32mStarting shell...\x1B[0m\n");
            if (KTShell_Start(&shell_proc, shell_cmd, term->width, term->height)) {
                shell_mode = true;
                console.input_enabled = false;
                console.prompt_pending = false;
                SituationSetWindowTitle("KaOS - Shell");
                if (buffer_to_free) free(buffer_to_free);
                return;
            } else {
                KTerm_WriteString(term, "\x1B[31mError: Failed to start shell.\x1B[0m\n");
            }
        }
    } else if (strcmp(cmd, "help") == 0) {
        const char* help_text_page1 =
            "\x1B[1;36mKaOS KTerm Help - Page 1\x1B[0m\n"
            "\x1B[1;32mBasic Commands:\x1B[0m\n"
            "  \x1B[33mhelp\x1B[0m             - Show this help (help 2, help 3 for more)\n"
            "  \x1B[33mcls/clear\x1B[0m        - Clear screen\n"
            "  \x1B[33mecho [text...]\x1B[0m   - Echo text (or newline)\n"
            "  \x1B[33mtype <filepath>\x1B[0m  - Pipe file contents to terminal\n"
            "  \x1B[33mshell [cmd]\x1B[0m     - Start system shell\n"
            "  \x1B[33mfont [name]\x1B[0m     - List fonts or switch font\n"
            "  \x1B[33mhistory\x1B[0m          - Show command history\n"
            "  \x1B[33mexit/quit\x1B[0m        - Exit console\n";
        const char* help_text_page2 =
            "\x1B[1;36mKaOS KTerm Help - Page 2\x1B[0m\n"
            "\x1B[1;32mKTerm Library Diagnostics:\x1B[0m\n"
            "  \x1B[33mterm_status\x1B[0m      - Show KTerm_GetStatus(term)\n"
            "  \x1B[33mterm_vtlevel\x1B[0m     - Display VT compatibility level\n"
            "  \x1B[33mterm_da\x1B[0m          - Request Device Attributes\n"
            "  \x1B[33mterm_diagbuffers\x1B[0m - Show buffer diagnostics\n"
            "  \x1B[33mset_fps <val>\x1B[0m      - Set pipeline target FPS\n"
            "  \x1B[33mset_budget <pct>\x1B[0m  - Set pipeline time budget\n"
            "\x1B[1;32mVT UI Widgets:\x1B[0m\n"
            "  \x1B[33mvt_styles\x1B[0m        - Box border/color style gallery\n"
            "  \x1B[33mvt_box\x1B[0m          - Draw a framed box (DEC/Unicode/color options)\n"
            "  \x1B[33mvt_menu\x1B[0m         - Interactive pulldown menu\n"
            "  \x1B[33mvt_combo\x1B[0m        - Collapsed combo field with expand list\n"
            "  \x1B[33mvt_dialog\x1B[0m       - OK/Cancel dialog box\n"
            "  \x1B[33mvt_styles_capture\x1B[0m - vt_styles + KTERM_CAPTURE_SCREENSHOT hook\n";
        const char* help_text_page3 =
            "\x1B[1;36mKaOS KTerm Help - Page 3\x1B[0m\n"
            "\x1B[1;32mFilesystem:\x1B[0m\n"
            "  \x1B[33mpwd\x1B[0m  \x1B[33mcd <path>\x1B[0m  \x1B[33mls [path]\x1B[0m  \x1B[33mdir [path]\x1B[0m\n"
            "\x1B[1;32mSystem Introspection:\x1B[0m\n"
            "  \x1B[33msysinfo\x1B[0m  \x1B[33mps\x1B[0m  \x1B[33mthreads\x1B[0m  \x1B[33msys_info\x1B[0m\n";
        if (token_count == 1 || (token_count == 2 && strcmp(tokens[1], "1") == 0)) {
            KTerm_WriteString(term, help_text_page1);
        } else if (token_count == 2 && strcmp(tokens[1], "2") == 0) {
            KTerm_WriteString(term, help_text_page2);
        } else if (token_count == 2 && strcmp(tokens[1], "3") == 0) {
            KTerm_WriteString(term, help_text_page3);
        } else {
            KTerm_WriteString(term, "\x1B[31mUsage: help [1|2|3]\x1B[0m\n");
        }
    } else {
        KTerm_WriteString(term, "\x1B[31mUnknown command: \x1B[0m");
        KTerm_WriteString(term, cmd);
        KTerm_WriteString(term, "\n\x1B[90mType 'help' for available commands.\x1B[0m\n");
    }

    console.prompt_pending = true;
    console.in_command = false;
    if (buffer_to_free) {
        free(buffer_to_free);
    }
}

#endif /* CONSOLE_COMMANDS_H */
