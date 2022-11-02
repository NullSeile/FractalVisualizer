#include "GLCore.h"
#include "MainLayer.h"

using namespace GLCore;

class FractalApp : public Application
{
public:
	FractalApp()
		: Application("Fractal Visualization")
	{
		GetWindow().SetIcon("assets/imgs/icon.png");
		PushLayer(new MainLayer());
	}
};

#if GLCORE_RELEASE && GLCORE_PLATFORM_WINDOWS
int wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
#else
int main()
#endif
{
	std::unique_ptr<FractalApp> app = std::make_unique<FractalApp>();
	app->Run();
}