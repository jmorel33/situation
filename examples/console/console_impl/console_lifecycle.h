#ifndef CONSOLE_LIFECYCLE_H
#define CONSOLE_LIFECYCLE_H

static void ConsoleShowWelcome(void) {
    KTerm_WriteString(term, "   \x1B[36m");
    KTerm_WriteChar(term, 0xC9);
    for (int i = 0; i < 74; i++) {
        KTerm_WriteChar(term, 0xCD);
    }
    KTerm_WriteChar(term, 0xBB);
    KTerm_WriteString(term, "\x1B[0m\n");
    KTerm_WriteString(term, "   \x1B[36m");
    KTerm_WriteChar(term, 0xBA);
    KTerm_WriteString(term, "\x1B[0m \x1B[1;37mKaizen Operating System\x1B[0m - \x1B[33mK-Term v2.7.14\x1B[0m Console                       \x1B[36m");
    KTerm_WriteChar(term, 0xBA);
    KTerm_WriteString(term, "\x1B[0m\n");
    KTerm_WriteString(term, "   \x1B[36m");
    KTerm_WriteChar(term, 0xC8);
    for (int i = 0; i < 74; i++) {
        KTerm_WriteChar(term, 0xCD);
    }
    KTerm_WriteChar(term, 0xBC);
    KTerm_WriteString(term, "\x1B[0m\n\n");
    KTerm_WriteString(term, "   \x1B[32mWelcome to KaOS!\x1B[0m Type \x1B[1;33mhelp\x1B[0m for available commands.\n\n");
}

bool Console_Init(const ConsoleConfig* config) {
    ConsoleConfig cfg = config ? *config : (ConsoleConfig)CONSOLE_CONFIG_DEFAULT;
    if (cfg.term_cols <= 0) {
        cfg.term_cols = 80;
    }
    if (cfg.term_rows <= 0) {
        cfg.term_rows = 50;
    }
    if (cfg.settle_delay_ms < 0) {
        cfg.settle_delay_ms = 0;
    }

    if (console_initialized) {
        return true;
    }

    if (SituationGetInitState() != SITUATION_STATE_READY) {
        CONSOLE_LOG("FATAL: Situation not in READY state before Console_Init");
        return false;
    }

    CONSOLE_LOG("Waiting for render thread to settle...");
#if defined(_WIN32)
    if (cfg.settle_delay_ms > 0) {
        Sleep((DWORD)cfg.settle_delay_ms);
    }
#else
    (void)cfg.settle_delay_ms;
#endif

    CONSOLE_LOG("Creating K-Term...");
    KTermConfig term_config = {
        .width = cfg.term_cols,
        .height = cfg.term_rows,
        .input_buffer_size = 4 * 1024 * 1024
    };

    term = KTerm_Create(term_config);
    CONSOLE_LOG("K-Term created: %p", (void*)term);
    if (!term) {
        CONSOLE_LOG("FATAL: Failed to create K-Term");
        return false;
    }

    KTerm_SetOutputSink(term, HandleKTermResponse, term);
    KTerm_SetTitleCallback(term, HandleTitleChange);
    KTerm_SelectCharacterSet(term, 1, CHARSET_CP437);
    KTerm_WriteString(term, "\x1B~");
    KTerm_WriteString(term, "\x1B[?1003h");
    KTerm_WriteString(term, "\x1B[?1006h");

    ConsoleShowWelcome();
    ShowPrompt();

    console.prompt_pending = false;
    console.in_command = false;
    console.line_ready = false;
    console.history_count = 0;
    console.history_pos = 0;
    console.echo_enabled = true;
    console.input_enabled = false;
    ClearEditBuffer();

    should_exit = false;
    capture_frame = 0;
    capture_path = getenv("KTERM_CAPTURE_SCREENSHOT");
    capture_exit = getenv("KTERM_CAPTURE_EXIT") != NULL;
    console_initialized = true;
    CONSOLE_LOG("Console_Init complete");
    return true;
}

bool Console_ShouldExit(void) {
    return should_exit;
}

void Console_Update(void) {
    if (!console_initialized || !term) {
        return;
    }

    if (SituationIsWindowResized()) {
        int w, h;
        SituationGetWindowSize(&w, &h);
        int cols = w / (DEFAULT_CHAR_WIDTH * 2);
        int rows = h / (DEFAULT_CHAR_HEIGHT * 2);
        KTerm_Resize(term, cols, rows);
        if (shell_mode) {
            KTShell_Resize(&shell_proc, cols, rows);
        }
        if (editor.active) {
            EditorRelayout();
        }
    }

    if (shell_mode) {
        char shell_buf[4096];
        size_t n = KTShell_Read(&shell_proc, shell_buf, sizeof(shell_buf));
        if (n > 0) {
            KTerm_PushInput(term, shell_buf, n);
        }
        if (!KTShell_IsRunning(&shell_proc)) {
            shell_mode = false;
            KTShell_Stop(&shell_proc);
            KTerm_WriteString(term, "\n\x1B[33mShell exited.\x1B[0m\n");
            SituationSetWindowTitle("KaOS - Kaizen Operating System v0.1");
            console.prompt_pending = true;
            console.input_enabled = false;
            console.in_command = false;
            console.waiting_for_prompt_cursor_pos = false;
        }
    }

    if (!shell_mode && !editor.active && !ConsoleVtMenuIsActive() && console.prompt_pending && !console.in_command && !console.waiting_for_prompt_cursor_pos) {
        CONSOLE_LOG("CLI MainLoop: Calling ShowPrompt.");
        ShowPrompt();
    }

    if (shell_mode) {
        int rk;
        while ((rk = SituationGetKeyPressed()) != 0) {
            char seq[8] = {0};
            bool ctrl = SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) || SituationIsKeyDown(SIT_KEY_RIGHT_CONTROL);
            if (rk == SIT_KEY_ENTER) {
                seq[0] = '\r';
            } else if (rk == SIT_KEY_BACKSPACE) {
                seq[0] = '\x08';
            } else if (rk == SIT_KEY_TAB) {
                seq[0] = '\t';
            } else if (rk == SIT_KEY_ESCAPE) {
                seq[0] = '\x1B';
            } else if (rk == SIT_KEY_UP) {
                seq[0] = '\x1B';
                seq[1] = '[';
                seq[2] = 'A';
            } else if (rk == SIT_KEY_DOWN) {
                seq[0] = '\x1B';
                seq[1] = '[';
                seq[2] = 'B';
            } else if (rk == SIT_KEY_RIGHT) {
                seq[0] = '\x1B';
                seq[1] = '[';
                seq[2] = 'C';
            } else if (rk == SIT_KEY_LEFT) {
                seq[0] = '\x1B';
                seq[1] = '[';
                seq[2] = 'D';
            } else if (rk == SIT_KEY_HOME) {
                seq[0] = '\x1B';
                seq[1] = '[';
                seq[2] = 'H';
            } else if (rk == SIT_KEY_END) {
                seq[0] = '\x1B';
                seq[1] = '[';
                seq[2] = 'F';
            } else if (rk == SIT_KEY_DELETE) {
                seq[0] = '\x1B';
                seq[1] = '[';
                seq[2] = '3';
                seq[3] = '~';
            } else if (ctrl && rk >= 'A' && rk <= 'Z') {
                seq[0] = (char)(rk - 'A' + 1);
            } else if (ctrl && rk >= 'a' && rk <= 'z') {
                seq[0] = (char)(rk - 'a' + 1);
            }
            if (seq[0]) {
                KTShell_Write(&shell_proc, seq, strlen(seq));
            }
        }
        int ch;
        while ((ch = SituationGetCharPressed()) != 0) {
            char buf[4];
            if (ch < 0x80) {
                buf[0] = (char)ch;
                KTShell_Write(&shell_proc, buf, 1);
            }
        }
    } else {
        KTermSit_ProcessInput(term);
    }

    if (shell_mode) {
        KTermSit_UpdateMouse(term);
    }

    {
        Vector2 mpos = SituationGetMousePosition();
        int ww, wh;
        SituationGetWindowSize(&ww, &wh);
        bool mouse_in_window = (mpos.x >= 0 && mpos.y >= 0 && mpos.x < ww && mpos.y < wh);
        static bool cursor_hidden = false;
        if (mouse_in_window && !cursor_hidden) {
            SituationHideCursor();
            cursor_hidden = true;
        } else if (!mouse_in_window && cursor_hidden) {
            SituationShowCursor();
            cursor_hidden = false;
        }
    }

    KTerm_Update(term);

#ifdef KTERM_STANDALONE_MODE
    KTerm_Draw(term);
    if (capture_path && ++capture_frame == 30) {
        SituationTakeScreenshot(capture_path);
        if (capture_exit) {
            should_exit = true;
        }
    }
#else
    SituationAcquireFrameCommandBuffer();
    KTerm_Draw(term);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SituationRenderVirtualDisplays(cmd);
    SituationEndFrame();
#endif
    if (capture_path && ++capture_frame == 30) {
        SituationTakeScreenshot(capture_path);
        if (capture_exit) {
            should_exit = true;
        }
    }
}

void Console_Shutdown(void) {
    if (!console_initialized) {
        return;
    }

    if (shell_mode) {
        KTShell_Stop(&shell_proc);
        shell_mode = false;
    }

    if (term) {
        KTerm_Destroy(term);
        term = NULL;
    }

    if (editor.undo_buffer) {
        free(editor.undo_buffer);
        editor.undo_buffer = NULL;
    }

    should_exit = false;
    console_initialized = false;
    capture_frame = 0;
    capture_path = NULL;
    capture_exit = false;
}

#endif /* CONSOLE_LIFECYCLE_H */
