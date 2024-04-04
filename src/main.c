#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/cglm.h>
#include <stdio.h>

#include "lib/Finger.h"
#include "lib/Keyboard.h"
#include "lib/SDL.h"
#include "lib/Timer.h"
#include "lib/Vulkan.h"
#include "lib/Window.h"

static char* WINDOW_TITLE = "Paint";
static char* ENGINE_NAME = "MS2024";
static u16 WINDOW_WIDTH = 800;
static u16 WINDOW_HEIGHT = 800;

static const u8 PHYSICS_FPS = 50;
static const u8 RENDER_FPS = 60;

static bool isCanvasDirty = false;

static bool isVBODirty = true;
static bool isUBODirty[] = {true, true};

static Vulkan_t s_Vulkan;
static Window_t s_Window;

typedef struct {
  vec2 vertex;
} Mesh_t;

typedef struct {
  vec3 pos;
  vec3 rot;
  vec3 scale;
} Instance_t;

#define MAX_INSTANCES 2  // TODO: find out how to exceed this limit
static u16 instanceCount = 1;
static Instance_t instances[MAX_INSTANCES];

typedef struct {
  vec3 cam;
  vec3 look;
  f32 aspect;
} World_t;

static World_t world;

static vec3 VEC3_Y_UP = {0, 1, 0};

typedef struct {
  mat4 proj;
  mat4 view;
} ubo_ProjView_t;

static Mesh_t vertices[] = {
    {{-0.5f, -0.5f}},
    {{0.5f, -0.5f}},
    {{0.5f, 0.5f}},
    {{-0.5f, 0.5f}},
};

static u16 indices[] = {0, 1, 2, 2, 3, 0};

static const char* shaderFiles[] = {
    "../assets/shaders/simple_shader.frag.spv",
    "../assets/shaders/simple_shader.vert.spv",
};

static const char* textureFiles[] = {
    "../assets/textures/canvas.png",
};

static ubo_ProjView_t ubo1;  // projection x view matrices

static void physicsCallback(const f64 deltaTime);
static void renderCallback(const f64 deltaTime);
static void keyboardCallback();
static void fingerCallback();

#define ATTR_COUNT 4

u32 urandom(u32 a, u32 b) {
  return a + ((rand() / (f32)RAND_MAX) * (b - a));
}

u32 canvas_size = 0;
u8* canvas;
VkBuffer buf;
VkDeviceMemory bufMemory;
void* data;

int main() {
  canvas_size = sizeof(u8) * WINDOW_WIDTH * WINDOW_HEIGHT * 4;
  canvas = (u8*)malloc(canvas_size);

  printf("begin main.\n");

  Timer__MeasureCycles();

  // initialize random seed using current time
  srand(Timer__NowMilliseconds());

  Vulkan__InitDriver1(&s_Vulkan);

  Window__New(&s_Window, WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, &s_Vulkan);
  SDL__Init();

  Keyboard__RegisterCallback(keyboardCallback);
  Finger__RegisterCallback(fingerCallback);

  Window__Begin(&s_Window);

  Vulkan__AssertDriverValidationLayersSupported(&s_Vulkan);

#if OS_MAC == 1
  ASSERT(s_Vulkan.m_requiredDriverExtensionsCount < VULKAN_REQUIRED_DRIVER_EXTENSIONS_CAP)
  // enable MoltenVK support for MacOS cross-platform support
  s_Vulkan.m_requiredDriverExtensions[s_Vulkan.m_requiredDriverExtensionsCount++] =
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#endif
  Vulkan__AssertDriverExtensionsSupported(&s_Vulkan);

  Vulkan__CreateInstance(&s_Vulkan, WINDOW_TITLE, ENGINE_NAME, 1, 0, 0);
  Vulkan__InitDriver2(&s_Vulkan);

  Vulkan__UsePhysicalDevice(&s_Vulkan, 0);
  Window__Bind(&s_Window);

  DrawableArea_t area = {0, 0};
  Window__GetDrawableAreaExtentBounds(&s_Window, &area);
  world.aspect = ASPECT_SQUARE_1_1;
  s_Vulkan.m_aspectRatio = world.aspect;
  Window__KeepAspectRatio(&s_Window, area.width, area.height);

  // establish vulkan scene
  Vulkan__AssertSwapChainSupported(&s_Vulkan);
  Vulkan__CreateLogicalDeviceAndQueues(&s_Vulkan);
  Vulkan__CreateSwapChain(&s_Vulkan, false);
  Vulkan__CreateImageViews(&s_Vulkan);
  Vulkan__CreateRenderPass(&s_Vulkan);
  Vulkan__CreateDescriptorSetLayout(&s_Vulkan);
  Vulkan__CreateGraphicsPipeline(
      &s_Vulkan,
      shaderFiles[0],
      shaderFiles[1],
      sizeof(Mesh_t),
      sizeof(Instance_t),
      ATTR_COUNT,
      (u32[ATTR_COUNT]){0, 1, 1, 1},
      (u32[ATTR_COUNT]){0, 1, 2, 3},
      (u32[ATTR_COUNT]){
          VK_FORMAT_R32G32_SFLOAT,
          VK_FORMAT_R32G32B32_SFLOAT,
          VK_FORMAT_R32G32B32_SFLOAT,
          VK_FORMAT_R32G32B32_SFLOAT},
      (u32[ATTR_COUNT]){
          offsetof(Mesh_t, vertex),
          offsetof(Instance_t, pos),
          offsetof(Instance_t, rot),
          offsetof(Instance_t, scale)});
  Vulkan__CreateFrameBuffers(&s_Vulkan);
  Vulkan__CreateCommandPool(&s_Vulkan);

  DEBUG_TRACE

  DEBUG_TRACE
  Vulkan__CreateTextureImageRW(
      &s_Vulkan,
      WINDOW_WIDTH,
      WINDOW_HEIGHT,
      canvas_size,
      canvas,
      &buf,
      &bufMemory,
      &data);

  DEBUG_TRACE
  for (u32 i = 0; i < 100; i += 4) {
    canvas[i + 0] = urandom(0, 0);    // R
    canvas[i + 1] = urandom(0, 255);  // G
    canvas[i + 2] = urandom(0, 255);  // B
    canvas[i + 3] = urandom(0, 255);  // A
  }
  DEBUG_TRACE
  Vulkan__UpdateTextureImageRW(
      &s_Vulkan,
      WINDOW_WIDTH,
      WINDOW_HEIGHT,
      canvas_size,
      canvas,
      &buf,
      &bufMemory,
      &data);

  DEBUG_TRACE
  // Vulkan__CreateTextureImage(&s_Vulkan, textureFiles[0]);
  Vulkan__CreateTextureImageView(&s_Vulkan);
  Vulkan__CreateTextureSampler(&s_Vulkan);
  Vulkan__CreateVertexBuffer(&s_Vulkan, 0, sizeof(vertices), vertices);
  Vulkan__CreateVertexBuffer(&s_Vulkan, 1, sizeof(instances), instances);
  Vulkan__CreateIndexBuffer(&s_Vulkan, sizeof(indices), indices);
  Vulkan__CreateUniformBuffers(&s_Vulkan, sizeof(ubo1));
  Vulkan__CreateDescriptorPool(&s_Vulkan);
  Vulkan__CreateDescriptorSets(&s_Vulkan);
  Vulkan__CreateCommandBuffers(&s_Vulkan);
  Vulkan__CreateSyncObjects(&s_Vulkan);
  s_Vulkan.m_drawIndexCount = ARRAY_COUNT(indices);

  // setup scene
  glm_vec3_copy((vec3){0, 0, 1}, world.cam);
  glm_vec3_copy((vec3){0, 0, 0}, world.look);

  glm_vec3_copy((vec3){0, 0, 0}, instances[0].pos);
  glm_vec3_copy((vec3){0, 0, 0}, instances[0].rot);
  glm_vec3_copy((vec3){1, 1, 1}, instances[0].scale);
  instanceCount = 1;

  // main loop
  Window__RenderLoop(&s_Window, PHYSICS_FPS, RENDER_FPS, &physicsCallback, &renderCallback);

  // cleanup
  printf("shutdown main.\n");
  Vulkan__DeviceWaitIdle(&s_Vulkan);
  Vulkan__Cleanup(&s_Vulkan);
  Window__Shutdown(&s_Window);
  printf("end main.\n");
  return 0;
}

static void keyboardCallback() {
  // LOG_DEBUGF(
  //     "SDL_KEY{UP,DOWN} state "
  //     "code %u location %u pressed %u alt %u "
  //     "ctrl %u shift %u meta %u",
  //     g_Keyboard__state.code,
  //     g_Keyboard__state.location,
  //     g_Keyboard__state.pressed,
  //     g_Keyboard__state.altKey,
  //     g_Keyboard__state.ctrlKey,
  //     g_Keyboard__state.shiftKey,
  //     g_Keyboard__state.metaKey);

  if (41 == g_Keyboard__state.code) {  // ESC
    s_Window.quit = true;
  }
}

static bool isFingerDown = false;
static u16 fingerX = 0;
static u16 fingerY = 0;

static void fingerCallback() {
  if (0 == g_Finger__state.x && 0 == g_Finger__state.y) return;

  // LOG_DEBUGF(
  //     "SDL_FINGER state "
  //     "event %s "
  //     "clicks %u pressure %2.5f touchId %u finger %u "
  //     "x %u y %u x_rel %d y_rel %d wheel_x %2.5f wheel_y %2.5f "
  //     "button_l %d button_m %d button_r %d button_x1 %d button_x2 %d ",
  //     (g_Finger__state.event == FINGER_UP       ? "UP"
  //      : g_Finger__state.event == FINGER_DOWN   ? "DOWN"
  //      : g_Finger__state.event == FINGER_MOVE   ? "MOVE"
  //      : g_Finger__state.event == FINGER_SCROLL ? "SCROLL"
  //                                               : ""),
  //     g_Finger__state.clicks,
  //     g_Finger__state.pressure,
  //     g_Finger__state.touchId,
  //     g_Finger__state.finger,
  //     g_Finger__state.x,
  //     g_Finger__state.y,
  //     g_Finger__state.x_rel,
  //     g_Finger__state.y_rel,
  //     g_Finger__state.wheel_x,
  //     g_Finger__state.wheel_y,
  //     g_Finger__state.button_l,
  //     g_Finger__state.button_m,
  //     g_Finger__state.button_r,
  //     g_Finger__state.button_x1,
  //     g_Finger__state.button_x2);

  if (FINGER_SCROLL == g_Finger__state.event) {
    // TODO: how to animate camera zoom with spring damping/smoothing?
    // TODO: how to move this into physics callback? or is it better not to?
    // world.cam[2] += -g_Finger__state.wheel_y * PLAYER_ZOOM_SPEED /* deltaTime*/;
    // isUBODirty[0] = true;
    // isUBODirty[1] = true;
  }

  else if (FINGER_DOWN == g_Finger__state.event) {
    // TODO: how to move this into physics callback? or is it better not to?
    // TODO: animate player walk-to, before placing-down
    // TODO: convert window x,y to world x,y

    // vec3 pos = (vec3){g_Finger__state.x, g_Finger__state.y, 0.0f};
    // mat4 pvMatrix;
    // glm_mat4_mul(ubo1.proj, ubo1.view, pvMatrix);
    // vec4 viewport = (vec4){0, 0, s_Window.width, s_Window.height};
    // vec3 dest;
    // glm_unproject(pos, pvMatrix, viewport, dest);
    // isVBODirty = true;

    isFingerDown = true;
    fingerX = g_Finger__state.x;
    fingerY = g_Finger__state.y;
  } else if (FINGER_UP == g_Finger__state.event) {
    isFingerDown = false;
    fingerX = g_Finger__state.x;
    fingerY = g_Finger__state.y;
  } else if (FINGER_MOVE == g_Finger__state.event) {
    if (g_Finger__state.pressure > 0) {
      isFingerDown = true;
    }
    fingerX = g_Finger__state.x;
    fingerY = g_Finger__state.y;
  }
}

int xy2i(u32 x, u32 y, u32 w, u8 sizet) {
  return ((y * w) + x) * sizet;
}

void physicsCallback(const f64 deltaTime) {
  // OnFixedUpdate(deltaTime);

  // perform blit calculations
  if (isFingerDown) {
    u32 BRUSH_SIZE = 16;
    u8 COLOR_R = urandom(0, 255);
    u8 COLOR_G = urandom(0, 255);
    u8 COLOR_B = urandom(0, 255);
    for (u32 x = 0; x < BRUSH_SIZE; x++) {
      for (u32 y = 0; y < BRUSH_SIZE; y++) {
        u32 i = xy2i(fingerX + x, fingerY + y, WINDOW_WIDTH, 4);
        if (i > canvas_size) continue;
        canvas[i + 0] = COLOR_R;  // R
        canvas[i + 1] = COLOR_G;  // G
        canvas[i + 2] = COLOR_B;  // B
        canvas[i + 3] = 255;      // A
      }
    }

    isCanvasDirty = true;
  }
}

static u8 newTexId;
static void renderCallback(const f64 deltaTime) {
  // OnUpdate(deltaTime);
  // TODO: perhaps might use this to render brush cursor

  if (isCanvasDirty) {
    isCanvasDirty = false;
    Vulkan__UpdateTextureImageRW(
        &s_Vulkan,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        canvas_size,
        canvas,
        &buf,
        &bufMemory,
        &data);
  }

  // character frame animation
  if (isVBODirty) {
    isVBODirty = false;

    s_Vulkan.m_instanceCount = instanceCount;
    Vulkan__UpdateVertexBuffer(&s_Vulkan, 1, sizeof(instances), instances);
  }

  if (isUBODirty[s_Vulkan.m_currentFrame]) {
    isUBODirty[s_Vulkan.m_currentFrame] = false;

    glm_lookat(
        world.cam,
        world.look,
        VEC3_Y_UP,  // Y-axis points upwards (GLM default)
        ubo1.view);

    s_Vulkan.m_aspectRatio = world.aspect;  // sync viewport

    // glm_perspective(
    //     glm_rad(45.0f),  // half the actual 90deg fov
    //     world.aspect,
    //     0.1f,  // TODO: adjust clipping range for z depth?
    //     10.0f,
    //     ubo1.proj);

    glm_ortho(-0.5f, +0.5f, -0.5f, +0.5f, 0.1f, 10.0f, ubo1.proj);

    // TODO: not sure i make use of one UBO per frame, really
    Vulkan__UpdateUniformBuffer(&s_Vulkan, s_Vulkan.m_currentFrame, &ubo1);
  }
}
