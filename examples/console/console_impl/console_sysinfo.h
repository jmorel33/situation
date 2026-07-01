#ifndef CONSOLE_SYSINFO_H
#define CONSOLE_SYSINFO_H

void SitHelperPrintDeviceInfo(void) {
    SituationCPUInfo cpu;
    SituationGPUInfo gpu;
    SituationMemoryInfo mem;
    SituationGetCPUInfo(&cpu);
    SituationGetGPUInfo(&gpu);
    SituationGetMemoryInfo(&mem);

    KTerm_WriteString(term, "  \x1B[1;34mCPU:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Name: \x1B[37m%s\x1B[0m\n", cpu.name);
    KTerm_WriteFormat(term, "    Threads: \x1B[37m%u\x1B[0m  Cores: \x1B[37m%u\x1B[0m\n", cpu.thread_count, cpu.core_count);
    KTerm_WriteFormat(term, "    Clock Speed: \x1B[37m%.2f GHz\x1B[0m\n", cpu.clock_speed_ghz);

    KTerm_WriteString(term, "  \x1B[1;34mGPU:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Name: \x1B[37m%s\x1B[0m\n", gpu.name);
    KTerm_WriteFormat(term, "    Dedicated VRAM: \x1B[37m%llu MB\x1B[0m\n", gpu.dedicated_memory_bytes / (1024 * 1024));

    KTerm_WriteString(term, "  \x1B[1;34mRAM:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Total: \x1B[37m%llu MB\x1B[0m\n", mem.total_bytes / (1024 * 1024));
    KTerm_WriteFormat(term, "    Available: \x1B[37m%llu MB\x1B[0m\n", mem.available_bytes / (1024 * 1024));

    int storage_count = SituationGetStorageDeviceCount();
    KTerm_WriteFormat(term, "  \x1B[1;34mStorage Devices (%d found):\x1B[0m\n", storage_count);
    for (int i = 0; i < storage_count; ++i) {
        char storage_name[SITUATION_MAX_DEVICE_NAME_LEN];
        uint64_t capacity_bytes = 0;
        uint64_t free_bytes = 0;
        if (!SituationGetStorageDevice(i, storage_name, sizeof(storage_name), &capacity_bytes, &free_bytes)) {
            continue;
        }
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, storage_name);
        KTerm_WriteFormat(term, "        Capacity: \x1B[37m%llu GB\x1B[0m\n", capacity_bytes / (1024 * 1024 * 1024));
        KTerm_WriteFormat(term, "        Free Space: \x1B[37m%llu GB\x1B[0m\n", free_bytes / (1024 * 1024 * 1024));
    }

    int network_count = SituationGetNetworkAdapterCount();
    KTerm_WriteFormat(term, "  \x1B[1;34mNetwork Adapters (%d found):\x1B[0m\n", network_count);
    for (int i = 0; i < network_count; ++i) {
        char adapter_name[SITUATION_MAX_DEVICE_NAME_LEN];
        if (!SituationGetNetworkAdapterName(i, adapter_name, sizeof(adapter_name))) {
            continue;
        }
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, adapter_name);
    }

    int input_count = SituationGetInputDeviceCount();
    KTerm_WriteFormat(term, "  \x1B[1;34mInput Devices (%d found):\x1B[0m\n", input_count);
    for (int i = 0; i < input_count; ++i) {
        char input_name[SITUATION_MAX_DEVICE_NAME_LEN];
        if (!SituationGetInputDeviceName(i, input_name, sizeof(input_name))) {
            continue;
        }
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, input_name);
    }
    KTerm_WriteString(term, "\x1B[0m");
}

void SitHelperPrintDisplayInfo(SituationDisplayInfo* displays, int count) {
    if (!displays || count == 0) {
        KTerm_WriteString(term, "  \x1B[31mNo display information available.\x1B[0m\n");
        return;
    }
    KTerm_WriteFormat(term, "  Found \x1B[1;37m%d\x1B[0m physical display(s):\n", count);
    for (int i = 0; i < count; ++i) {
        KTerm_WriteFormat(term, "  \x1B[1;34mDisplay [%d]:\x1B[0m \x1B[37m%s\x1B[0m\n", i, displays[i].name);
        KTerm_WriteFormat(term, "    Primary: \x1B[37m%s\x1B[0m\n", displays[i].is_primary ? "Yes" : "No");
        KTerm_WriteFormat(term, "    Current Mode: \x1B[37m%dx%d @ %dHz, %d-bit\x1B[0m\n",
               displays[i].current_mode.width, displays[i].current_mode.height,
               displays[i].current_mode.refresh_rate, displays[i].current_mode.color_depth);
        KTerm_WriteFormat(term, "    Available Modes (\x1B[37m%d\x1B[0m found):\n", displays[i].available_mode_count);
        for (int j = 0; j < displays[i].available_mode_count; ++j) {
            if (j < 3 || j > displays[i].available_mode_count - 2) {
                 KTerm_WriteFormat(term, "      - \x1B[37m%dx%d @ %dHz, %d-bit\x1B[0m\n",
                       displays[i].available_modes[j].width, displays[i].available_modes[j].height,
                       displays[i].available_modes[j].refresh_rate, displays[i].available_modes[j].color_depth);
            } else if (j == 3 && displays[i].available_mode_count > 4) {
                KTerm_WriteFormat(term, "      - \x1B[90m... (and %d more)\x1B[0m\n", displays[i].available_mode_count - 4);
            }
        }
    }
    KTerm_WriteString(term, "\x1B[0m");
}

void SitHelperPrintAudioDeviceInfo(SituationAudioDeviceInfo* devices, int count) {
    if (!devices || count == 0) {
        KTerm_WriteString(term, "  \x1B[31mNo audio device information available.\x1B[0m\n");
        return;
    }
    KTerm_WriteFormat(term, "  Found \x1B[1;37m%d\x1B[0m audio playback device(s):\n", count);
    for (int i = 0; i < count; ++i) {
        KTerm_WriteFormat(term, "  \x1B[1;34mDevice [%d]\x1B[0m (ID: \x1B[37m%s\x1B[0m): \x1B[37m%s\x1B[0m\n", i, devices[i].id, devices[i].name);
        KTerm_WriteFormat(term, "    Default Playback: \x1B[37m%s\x1B[0m\n", devices[i].is_default_playback ? "Yes" : "No");
    }
    KTerm_WriteString(term, "\x1B[0m");
}

#endif /* CONSOLE_SYSINFO_H */
