#include "Application.h"

#if defined(_DEBUG)
#include <initguid.h>
DEFINE_GUID(DXGI_DEBUG_ALL, 0xe48ae283, 0xda80, 0x490b, 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8);

void ReportLiveObjects()
{
	ComPtr<IDXGIDebug1> dxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
}
#endif

int main(int argc, char const *argv[])
{
	Log::Init();
	LOG_WARN("Initialized.");

#if defined(_DEBUG)
	std::atexit(ReportLiveObjects);
#endif

	Application().Run();
	return 0;
}