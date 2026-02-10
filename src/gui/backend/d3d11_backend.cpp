/**
 * @file    d3d11_backend.cpp
 * @brief   Direct3D 11 Render Backend Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Uses WIL (Windows Implementation Libraries) for:
 * - wil::com_ptr: Modern COM smart pointer with better semantics than WRL
 * - HRESULT error handling macros
 */

#ifdef _WIN32

#include "gui/backend/d3d11_backend.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_dx11.h>

#include <wil/com.h>
#include <wil/result.h>

#include <spdlog/spdlog.h>

// Link D3D11 libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace gwt::gui {

// =============================================================================
// Static Availability Check
// =============================================================================

bool D3D11Backend::is_available() noexcept {
    try {
        wil::com_ptr<ID3D11Device> device;
        wil::com_ptr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL feature_level;

        UINT flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL feature_levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        // Try hardware first
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            feature_levels,
            _countof(feature_levels),
            D3D11_SDK_VERSION,
            &device,
            &feature_level,
            &context
        );

        if (SUCCEEDED(hr)) {
            return true;
        }

        // Try WARP (Windows Advanced Rasterization Platform) as fallback
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            feature_levels,
            _countof(feature_levels),
            D3D11_SDK_VERSION,
            &device,
            &feature_level,
            &context
        );

        if (SUCCEEDED(hr)) {
            spdlog::info("D3D11: Hardware not available, WARP fallback available");
            return true;
        }

        return false;
    } catch (...) {
        return false;
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

D3D11Backend::~D3D11Backend() {
    if (m_initialized) {
        shutdown();
    }
}

bool D3D11Backend::init(SDL_Window* window) {
    if (m_initialized) {
        spdlog::warn("D3D11 backend already initialized");
        return true;
    }

    if (!window) {
        spdlog::error("Null window provided to D3D11 backend");
        set_error(BackendError::InitFailed);
        return false;
    }

    m_window = window;

    // Get HWND from SDL window
    m_hwnd = static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        nullptr
    ));

    if (!m_hwnd) {
        spdlog::error("Failed to get HWND from SDL window");
        set_error(BackendError::InitFailed);
        return false;
    }

    // Get window size
    SDL_GetWindowSizeInPixels(window, &m_window_width, &m_window_height);

    // Setup swap chain description
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = m_window_width;
    sd.Height = m_window_height;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Stereo = FALSE;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Create device and swap chain
    UINT create_device_flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    // First, create the device
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        create_device_flags,
        feature_levels,
        _countof(feature_levels),
        D3D11_SDK_VERSION,
        &m_device,
        &m_feature_level,
        &m_context
    );

    // Fallback to WARP if hardware fails
    if (FAILED(hr)) {
        spdlog::warn("D3D11: Hardware device creation failed, trying WARP");
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            create_device_flags,
            feature_levels,
            _countof(feature_levels),
            D3D11_SDK_VERSION,
            &m_device,
            &m_feature_level,
            &m_context
        );
    }

    if (FAILED(hr)) {
        spdlog::error("D3D11: Failed to create device: 0x{:08X}", static_cast<unsigned>(hr));
        set_error(BackendError::InitFailed);
        return false;
    }

    // Get DXGI factory from device
    auto dxgi_device = m_device.query<IDXGIDevice>();
    if (!dxgi_device) {
        spdlog::error("D3D11: Failed to get DXGI device");
        set_error(BackendError::InitFailed);
        return false;
    }

    wil::com_ptr<IDXGIAdapter> adapter;
    hr = dxgi_device->GetAdapter(&adapter);
    if (FAILED(hr)) {
        spdlog::error("D3D11: Failed to get DXGI adapter");
        set_error(BackendError::InitFailed);
        return false;
    }

    wil::com_ptr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        spdlog::error("D3D11: Failed to get DXGI factory");
        set_error(BackendError::InitFailed);
        return false;
    }

    // Create swap chain
    hr = factory->CreateSwapChainForHwnd(
        m_device.get(),
        m_hwnd,
        &sd,
        nullptr,
        nullptr,
        &m_swap_chain
    );

    if (FAILED(hr)) {
        spdlog::error("D3D11: Failed to create swap chain: 0x{:08X}", static_cast<unsigned>(hr));
        set_error(BackendError::InitFailed);
        return false;
    }

    // Disable ALT+ENTER fullscreen toggle
    factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Create render target view
    if (!create_render_target()) {
        spdlog::error("D3D11: Failed to create render target");
        set_error(BackendError::InitFailed);
        return false;
    }

    // Log D3D11 info
    DXGI_ADAPTER_DESC adapter_desc;
    adapter->GetDesc(&adapter_desc);

    char adapter_name[128];
    WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, -1,
                        adapter_name, sizeof(adapter_name), nullptr, nullptr);

    spdlog::info("D3D11 initialized:");
    spdlog::info("  Adapter: {}", adapter_name);
    spdlog::info("  Feature Level: {}.{}",
                 (m_feature_level >> 12) & 0xF,
                 (m_feature_level >> 8) & 0xF);
    spdlog::info("  Dedicated VRAM: {} MB", adapter_desc.DedicatedVideoMemory / (1024 * 1024));

    m_initialized = true;
    clear_error();
    return true;
}

void D3D11Backend::shutdown() {
    if (!m_initialized) return;

    // Destroy all textures
    m_textures.clear();

    // Cleanup render target
    cleanup_render_target();

    // Release D3D11 objects (wil::com_ptr handles release automatically)
    m_swap_chain.reset();
    m_context.reset();
    m_device.reset();

    m_window = nullptr;
    m_hwnd = nullptr;
    m_initialized = false;

    spdlog::debug("D3D11 backend shutdown complete");
}

bool D3D11Backend::create_render_target() {
    wil::com_ptr<ID3D11Texture2D> back_buffer;
    HRESULT hr = m_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateRenderTargetView(back_buffer.get(), nullptr, &m_rtv);
    return SUCCEEDED(hr);
}

void D3D11Backend::cleanup_render_target() {
    m_rtv.reset();
}

// =============================================================================
// ImGui Integration
// =============================================================================

void D3D11Backend::imgui_init() {
    if (!m_initialized) return;

    ImGui_ImplSDL3_InitForD3D(m_window);
    ImGui_ImplDX11_Init(m_device.get(), m_context.get());

    spdlog::debug("ImGui D3D11 backend initialized");
}

void D3D11Backend::imgui_shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

void D3D11Backend::imgui_new_frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void D3D11Backend::imgui_render() {
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// =============================================================================
// Frame Management
// =============================================================================

void D3D11Backend::begin_frame() {
    // Set render target
    ID3D11RenderTargetView* rtv = m_rtv.get();
    m_context->OMSetRenderTargets(1, &rtv, nullptr);

    // Set viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_window_width);
    vp.Height = static_cast<float>(m_window_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_context->RSSetViewports(1, &vp);

    // Clear screen
    const float clear_color[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.get(), clear_color);
}

void D3D11Backend::end_frame() {
    // Nothing special needed
}

void D3D11Backend::present() {
    // Present with vsync
    m_swap_chain->Present(1, 0);
}

void D3D11Backend::on_resize(int width, int height) {
    if (!m_initialized || width <= 0 || height <= 0) return;

    // Get actual drawable size
    SDL_GetWindowSizeInPixels(m_window, &m_window_width, &m_window_height);

    // Release render target before resize
    cleanup_render_target();
    m_context->Flush();

    // Resize swap chain buffers
    HRESULT hr = m_swap_chain->ResizeBuffers(
        0,  // Keep current buffer count
        m_window_width,
        m_window_height,
        DXGI_FORMAT_UNKNOWN,  // Keep current format
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
    );

    if (FAILED(hr)) {
        spdlog::error("D3D11: Failed to resize swap chain: 0x{:08X}", static_cast<unsigned>(hr));
        return;
    }

    // Recreate render target
    create_render_target();
}

// =============================================================================
// Texture Operations
// =============================================================================

DXGI_FORMAT D3D11Backend::get_dxgi_format(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGB8:
        case TextureFormat::RGBA8:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::BGR8:
        case TextureFormat::BGRA8:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        default:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

uint32_t D3D11Backend::get_bytes_per_pixel(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGB8:
        case TextureFormat::BGR8:
            return 3;
        case TextureFormat::RGBA8:
        case TextureFormat::BGRA8:
            return 4;
        default:
            return 4;
    }
}

TextureHandle
D3D11Backend::create_texture(const TextureDesc& desc, std::span<const uint8_t> data) {
    if (!m_initialized) {
        set_error(BackendError::InitFailed);
        return TextureHandle{};
    }

    // D3D11 doesn't support RGB8, need to convert to RGBA8
    std::vector<uint8_t> rgba_data;
    const uint8_t* pixel_data = nullptr;
    uint32_t row_pitch = desc.width * 4;  // Always RGBA for D3D11

    if (!data.empty()) {
        if (desc.format == TextureFormat::RGB8 || desc.format == TextureFormat::BGR8) {
            // Convert RGB to RGBA
            rgba_data.resize(desc.width * desc.height * 4);
            const uint8_t* src = data.data();
            uint8_t* dst = rgba_data.data();

            for (uint32_t i = 0; i < desc.width * desc.height; ++i) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
                src += 3;
                dst += 4;
            }
            pixel_data = rgba_data.data();
        } else {
            pixel_data = data.data();
        }
    }

    // Create texture
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = desc.width;
    tex_desc.Height = desc.height;
    tex_desc.MipLevels = desc.generate_mips ? 0 : 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = get_dxgi_format(desc.format);
    tex_desc.SampleDesc.Count = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (desc.generate_mips) {
        tex_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }
    tex_desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = pixel_data;
    init_data.SysMemPitch = row_pitch;

    wil::com_ptr<ID3D11Texture2D> texture;
    HRESULT hr = m_device->CreateTexture2D(
        &tex_desc,
        pixel_data ? &init_data : nullptr,
        &texture
    );

    if (FAILED(hr)) {
        spdlog::error("D3D11: Failed to create texture: 0x{:08X}", static_cast<unsigned>(hr));
        set_error(BackendError::TextureCreationFailed);
        return TextureHandle{};
    }

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = tex_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = desc.generate_mips ? static_cast<UINT>(-1) : 1;

    wil::com_ptr<ID3D11ShaderResourceView> srv;
    hr = m_device->CreateShaderResourceView(texture.get(), &srv_desc, &srv);

    if (FAILED(hr)) {
        spdlog::error("D3D11: Failed to create SRV: 0x{:08X}", static_cast<unsigned>(hr));
        set_error(BackendError::TextureCreationFailed);
        return TextureHandle{};
    }

    // Generate mipmaps if requested
    if (desc.generate_mips && pixel_data) {
        m_context->GenerateMips(srv.get());
    }

    // Create handle
    TextureHandle handle{m_next_handle_id++};
    m_textures[handle.id] = TextureData{
        std::move(texture),
        std::move(srv),
        desc
    };

    spdlog::debug("D3D11: Created texture {} ({}x{})", handle.id, desc.width, desc.height);

    clear_error();
    return handle;
}

void D3D11Backend::update_texture(TextureHandle handle, std::span<const uint8_t> data) {
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        spdlog::warn("D3D11: Attempted to update invalid texture handle: {}", handle.id);
        return;
    }

    auto& tex = it->second;

    // Convert RGB to RGBA if needed
    std::vector<uint8_t> rgba_data;
    const uint8_t* pixel_data = data.data();

    if (tex.desc.format == TextureFormat::RGB8 || tex.desc.format == TextureFormat::BGR8) {
        rgba_data.resize(tex.desc.width * tex.desc.height * 4);
        const uint8_t* src = data.data();
        uint8_t* dst = rgba_data.data();

        for (uint32_t i = 0; i < tex.desc.width * tex.desc.height; ++i) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
            src += 3;
            dst += 4;
        }
        pixel_data = rgba_data.data();
    }

    // Update texture
    D3D11_BOX box = {};
    box.left = 0;
    box.top = 0;
    box.front = 0;
    box.right = tex.desc.width;
    box.bottom = tex.desc.height;
    box.back = 1;

    m_context->UpdateSubresource(
        tex.texture.get(),
        0,
        &box,
        pixel_data,
        tex.desc.width * 4,  // Always RGBA
        0
    );
}

void D3D11Backend::destroy_texture(TextureHandle handle) {
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        return;
    }

    m_textures.erase(it);
    spdlog::debug("D3D11: Destroyed texture {}", handle.id);
}

void* D3D11Backend::get_imgui_texture_id(TextureHandle handle) const {
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        return nullptr;
    }

    // ImGui expects ID3D11ShaderResourceView* as texture ID
    return it->second.srv.get();
}

// =============================================================================
// Backend Info
// =============================================================================

std::string_view D3D11Backend::name() const noexcept {
    switch (m_feature_level) {
        case D3D_FEATURE_LEVEL_11_1: return "Direct3D 11.1";
        case D3D_FEATURE_LEVEL_11_0: return "Direct3D 11.0";
        case D3D_FEATURE_LEVEL_10_1: return "Direct3D 10.1";
        case D3D_FEATURE_LEVEL_10_0: return "Direct3D 10.0";
        default: return "Direct3D 11";
    }
}

}  // namespace gwt::gui

#endif  // _WIN32
