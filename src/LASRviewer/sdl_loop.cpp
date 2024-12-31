#include <SDL2/SDL.h>

#include <thread>
#include <atomic>

#include "PointCloud.h"
#include "drawer.h"
#include "sdlglutils.h"

#include "imgui.h"
#include "ImGui/backends/imgui_impl_sdl2.h"
#include "ImGui/backends/imgui_impl_opengl2.h"

const char *hand1[] =
{
  /* width height num_colors chars_per_pixel */
  " 16 16 3 1 ",
  /* colors */
  "X c #000000",
  ". c #ffffff",
  "  c None",
  /* pixels */
  "       XX       ",
  "   XX X..XXX    ",
  "  X..XX..X..X   ",
  "  X..XX..X..X X ",
  "   X..X..X..XX.X",
  "   X..X..X..X..X",
  " XX X.......X..X",
  "X..XX..........X",
  "X...X.........X ",
  " X............X ",
  "  X...........X ",
  "  X..........X  ",
  "   X.........X  ",
  "    X.......X   ",
  "     X......X   ",
  "     X......X   ",
  "0,0"
};

const char *hand2[] =
  {
  /* width height num_colors chars_per_pixel */
  " 16 16 3 1 ",
  /* colors */
  "X c #000000",
  ". c #ffffff",
  "  c None",
  /* pixels */
  "                ",
  "                ",
  "                ",
  "                ",
  "    XX XX XX    ",
  "   X..X..X..XX  ",
  "   X........X.X ",
  "    X.........X ",
  "   XX.........X ",
  "  X...........X ",
  "  X...........X ",
  "  X..........X  ",
  "   X.........X  ",
  "    X.......X   ",
  "     X......X   ",
  "     X......X   ",
  "0,0"
  };

const char *move[] =
  {
  /* width height num_colors chars_per_pixel */
  " 16 16 1 ",
  /* colors */
  "X c #000000",
  ". c #ffffff",
  "  c None",
  /* pixels */
  "       XX       ",
  "      X..X      ",
  "     X....X     ",
  "    X......X    ",
  "   X XX..XX X   ",
  "  X.X X..X X.X  ",
  " X..XXX..XXX..X ",
  "X..............X",
  "X..............X",
  " X..XXX..XXX..X ",
  "  X.X X..X X.X  ",
  "   X XX..XX X   ",
  "    X......X    ",
  "     X....X     ",
  "      X..X      ",
  "       XX       ",
  "0,0"
  };



const Uint32 time_per_frame = 1000 / 30;

void sdl_loop(PointCloud* las)
{
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
  {
    eprint("Error: %s\n", SDL_GetError());
    return;
  }

  // From 2.0.18: Enable native IME.
  #ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
  #endif

  unsigned int width = 600;
  unsigned int height = 600;
  bool run = true;
  Uint32 last_time, current_time, elapsed_time;

  // Setup window
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8); // 4x antialiasing
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); // Enable the sample buffer
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* window = SDL_CreateWindow("lasR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
  if (window == nullptr)
  {
    eprint("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return;
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1); // Enable vsync

  SDL_Cursor* _hand1 = cursorFromXPM(hand1);
  SDL_Cursor* _hand2 = cursorFromXPM(hand2);
  SDL_Cursor* _move  = cursorFromXPM(move);
  SDL_SetCursor(_hand1);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL2_Init();

  Drawer *drawer = new Drawer(window, las);
  drawer->camera.setRotateSensivity(0.1);
  drawer->camera.setZoomSensivity(10);
  drawer->camera.setPanSensivity(1);
  drawer->setPointSize(4);

  last_time = SDL_GetTicks();

  // UI constant
  int num_attributes = las->header->schema.num_attributes()-3;
  std::vector<const char*> attributes;
  for (size_t i = 3; i < las->header->schema.num_attributes(); i++) { attributes.push_back(las->header->schema.attributes[i].name.c_str()); }

  // State Variables
  float pointSize = 4.0f;
  float min_percentile = 0.01f;
  float max_percentile = 0.99f;
  float edl_strengh = 5.0f;
  bool showSpatialIndex = false;
  bool showEDL = true;
  int attributeSelection = 0;
  int point_budget = 2000;
  bool ctrlPressed = false;
  bool rotate = false;
  bool pan = false;

  while (run)
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL2_ProcessEvent(&event);

      //if (io.WantCaptureMouse || io.WantCaptureKeyboard)  break; // Let ImGui handle the input

      switch (event.type)
      {
        case SDL_QUIT:
        {
          run = false;
          break;
        }

        /*case SDL_KEYDOWN:
        {
          switch (event.key.keysym.sym)
          {
            case SDLK_LCTRL:
            case SDLK_RCTRL:
              ctrlPressed = true;
              break;
            case SDLK_ESCAPE:
              run = false;
              break;
            case SDLK_z:
              drawer->setAttribute(Drawer::AttributeEnum::Z);
              break;
            case SDLK_i:
              drawer->setAttribute(Drawer::AttributeEnum::I);
              break;
            case SDLK_c:
              drawer->setAttribute(Drawer::AttributeEnum::CLASS);
              break;
            case SDLK_r:
            case SDLK_g:
            case SDLK_b:
              drawer->setAttribute(Drawer::AttributeEnum::RGB);
              break;
            case SDLK_q:
              drawer->display_hide_spatial_index();
              break;
            case SDLK_l:
              drawer->display_hide_edl();
              break;
            case SDLK_PLUS:
            case SDLK_KP_PLUS:
            case SDLK_p:
              drawer->point_size_plus();
              break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
            case SDLK_m:
              drawer->point_size_minus();
              break;
        case SDLK_RIGHT:
              drawer->nextAttribute();
              drawer->setAttribute(Drawer::AttributeEnum::OTHER);
          break;
        case SDLK_LEFT:
            drawer->previousAttribute();
            drawer->setAttribute(Drawer::AttributeEnum::OTHER);
            break;
          }
          break;
        }*/

        /*case SDL_KEYUP:
        {
          switch (event.key.keysym.sym)
          {
            case SDLK_LCTRL:
            case SDLK_RCTRL:
              ctrlPressed = false;
              break;
          }
          break;
        }*/

        case SDL_MOUSEBUTTONUP:
        {
          switch(event.button.button)
          {
            case SDL_BUTTON_LEFT:
              rotate = false;
              SDL_SetCursor(_hand1);
              break;
            case SDL_BUTTON_RIGHT:
              pan = false;
              SDL_SetCursor(_hand1);
              break;
          }
          break;
        }

        case SDL_MOUSEBUTTONDOWN:
        {
          switch(event.button.button)
          {
            case SDL_BUTTON_LEFT:
              rotate = true;
              SDL_SetCursor(_hand2);
              break;
            case SDL_BUTTON_RIGHT:
              pan = true;
              SDL_SetCursor(_move);
              break;
          }
          break;
        }

        case SDL_MOUSEMOTION:
        {
          if (pan) drawer->camera.pan(event.motion.xrel, event.motion.yrel);
          if (rotate) drawer->camera.rotate(event.motion.xrel, event.motion.yrel);
          break;
        }

        case SDL_MOUSEWHEEL:
        {
          if (ctrlPressed && event.wheel.y > 0) drawer->budget_plus();
          else if (ctrlPressed && event.wheel.y < 0) drawer->budget_minus();
          else drawer->camera.zoom(event.wheel.y);
          break;
        }

        case SDL_WINDOWEVENT:
        {
          drawer->resize();
          break;
        }
      }
    }

    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
    {
      SDL_Delay(10);
      continue;
    }

    current_time = SDL_GetTicks();
    elapsed_time = current_time - last_time;

    if (elapsed_time < time_per_frame)
    {
      SDL_Delay(time_per_frame - elapsed_time);
      continue;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Rendering control
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove;

    ImGui::Begin("Viewer Controls");

    ImGui::Text("Point Size:");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adjust the size of the points.");
    ImGui::SliderFloat("##PointSizeSlider", &pointSize, 1.0f, 10.0f);

    ImGui::Text("Attributes:");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select the attribute to display.");
    ImGui::Combo("##AttributeCombo", &attributeSelection, attributes.data(), num_attributes);

    ImGui::Text("Contrast:");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adjust the contrast of the point cloud by clamping lower and upper percentiles.");
    ImGui::SliderFloat("##MinPercentileSlider", &min_percentile, 0.0f, 1.0f);
    ImGui::SliderFloat("##MaxPercentileSlider", &max_percentile, 0.0f, 1.0f);

    ImGui::Text("Eye Dome Lighting:");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable or adjust the Eye Dome Lighting strengh for better visualization of points.");
    ImGui::Checkbox("Enable", &showEDL);
    ImGui::SliderFloat("##EDLStrenghSlider", &edl_strengh, 1.0f, 30.0f);

    ImGui::Text("Point Budget:");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Maximum number of points to display (x1000)");
    ImGui::SliderInt("##BudgetSlider", &point_budget, 100, 100000);

    // Statistics panel
    ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 100), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGuiWindowFlags statsWindowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("Statistics", nullptr, statsWindowFlags);

    ImGui::Text("Displayed: %ldk/%ldk points (%.1f%%)", (long)drawer->rendered_points_count/1000, (long)las->npoints/1000, (double)drawer->rendered_points_count/(double)las->npoints * 100);
    ImGui::Text("Rendering: %.3f s (%.1f fps)", drawer->total_time, 1.0f / drawer->total_time);
    ImGui::Text("  Query: %.0f ms (%.1f fps, %.1f%%)", drawer->query_time*1000, 1.0f / drawer->query_time, drawer->query_time / drawer->total_time * 100);
    ImGui::Text("  Cloud: %.0f ms (%.1f fps, %.1f%%)",  drawer->rendering_time*1000, 1.0f / drawer->rendering_time, drawer->rendering_time / drawer->total_time * 100);
    ImGui::Text("  EDL: %.0f ms (%.1f fps, %.1f%%)",  drawer->edl_time*1000, 1.0f / drawer->edl_time, drawer->edl_time / drawer->total_time * 100);

    ImGui::End();

    drawer->setBudget(point_budget*1000);
    drawer->setEDL(showEDL);
    drawer->setEDLstrength(edl_strengh);
    drawer->setPointSize(pointSize);
    drawer->setPercentiles(min_percentile, max_percentile);
    drawer->setAttribute(attributeSelection+3);
    drawer->draw();

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);

    last_time = current_time;

  }

  // Cleanup
  ImGui_ImplOpenGL2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  delete drawer;
  SDL_SetCursor(NULL);
  SDL_FreeCursor(_hand1);
  SDL_FreeCursor(_hand2);
  SDL_FreeCursor(_move);
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

