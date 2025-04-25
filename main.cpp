/**
 * @title wayland-overlay
 * @file main.cpp
 * @author typedef
 */

/*
 * https://github.com/fantasy-cat/FC2T
 */
//#define FC2_TEAM_CONSTELLATION4
#include <fc2.hpp>

/**
 * SDL3 (https://aur.archlinux.org/packages/sdl3)
 *      - build on wayland environment
 *      - use aur package manager to do this or manually do it yourself.
 * Vulkan rendering
 * SDL3 TTF (https://github.com/libsdl-org/SDL_ttf)
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "SDL3_ttf/SDL_ttf.h"

/**
 * fmt library (https://archlinux.org/packages/extra-testing/x86_64/fmt/)
 */
#include <fmt/core.h>

/**
 * std::unordered_map
 */
#include <unordered_map>

/**
 * std::array
 */
#include <array>

/**
 * logging macro
 */
#ifndef log
    #define log(fmt_str, ...) fmt::println("[wayland-overlay] " fmt_str, ##__VA_ARGS__)
#endif

int main( )
{
    /**
     * fc2t check
     */
    const auto session_information = fc2::get_session();
    if( fc2::get_error() != FC2_TEAM_ERROR_NO_ERROR )
    {
        log( "solution doesn't appear to be open" );
        return -1;
    }
    log( "started. welcome {}", session_information.username );

    /**
     * get window dimensions from wayland_overlay.lua
     *
     * after, download the UbuntuMono font inside of FC2
     * and return the directory and file location for SDL3_TTF
     */
    std::array< unsigned int, 4 > window_dimensions = {};
    window_dimensions[ 0 ] = fc2::call< unsigned int >( "wayland_overlay_x", FC2_LUA_TYPE_INT );
    window_dimensions[ 1 ] = fc2::call< unsigned int >( "wayland_overlay_y", FC2_LUA_TYPE_INT );
    window_dimensions[ 2 ] = fc2::call< unsigned int >( "wayland_overlay_w", FC2_LUA_TYPE_INT );
    window_dimensions[ 3 ] = fc2::call< unsigned int >( "wayland_overlay_h", FC2_LUA_TYPE_INT );

    log(
        "window dimensions: {}, {} - {}x{}",
        window_dimensions[ 0 ],
        window_dimensions[ 1 ],
        window_dimensions[ 2 ],
        window_dimensions[ 3 ]
    );

    const auto font_path = fc2::call< std::string >( "wayland_overlay_font", FC2_LUA_TYPE_STRING );
    if( font_path.empty() )
    {
        log( "font may have been downloaded for the first time. restart" );
        return -1;
    }
    log( "font: {}", font_path );

    /**
     * initialize SDL
     */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
    if( !SDL_Init( SDL_INIT_VIDEO ) )
    {
        log( "sdl could not be initialized: {}\n", SDL_GetError() );
        std::getchar();
        return -1;
    }
    log( "sdl initialized" );

    /**
     * initialize TTF
     *
     * statically compiled inside of wayland-overlay
     * see /dependencies/
     */
    if( !TTF_Init() )
    {
        log( "ttf could not be initialized\n" );
        std::getchar();
        return -1;
    }
    log( "ttf initialized" );

    /**
     * create window and renderer
     * convert to unique_ptr after by transferring ownership
     *
     * we set the parent window to 1, 1 because SDL_CreatePopupWindow position
     * is relative to the parent window. without this, the overlay will create itself
     * wherever the DE tells it to create (usually where your cursor is or tiled.
     *
     * the reason for 1:1 is because on Hyprland (assuming other DEs do this too), if the position
     * is 0,0, the DE will automatically center the window or move it somewhere predetermined.
     * same applies for window size.
     */
    SDL_Window * _parent = SDL_CreateWindow(
        "fc2t overlay",
        0,
        0,
        0
    );

    if( !_parent )
    {
        log( "parent window could not be created: {}\n", SDL_GetError() );
        std::getchar();
        return -1;
    }

    SDL_SetWindowPosition(
        _parent,
        static_cast< int >( window_dimensions[ 0 ] ),
        static_cast< int >( window_dimensions[ 1 ] )
    );

    SDL_SetWindowSize(
        _parent,
        1,
        1
    );

    SDL_Window * _window = SDL_CreatePopupWindow(
        _parent,
        0,
        0,
        static_cast < int >( window_dimensions[ 2 ] ),
        static_cast < int >( window_dimensions[ 3 ] ),
        SDL_WINDOW_TOOLTIP |
        SDL_WINDOW_VULKAN |
        SDL_WINDOW_TRANSPARENT |
        SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_ALWAYS_ON_TOP
    );

    if( !_window )
    {
        _parent = nullptr;

        log( "popup window could not be created: {}\n", SDL_GetError() );
        std::getchar();
        return -1;
    }

    SDL_Renderer * _renderer = SDL_CreateRenderer(
        _window,
        nullptr
    );

    if( !_renderer)
    {
        _parent = nullptr;
        _window = nullptr;
        _renderer = nullptr;

        log( "renderer could not be created: {}\n", SDL_GetError() );
        std::getchar();
        return -1;
    }

    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> parent(
            _parent,
            SDL_DestroyWindow
    );

    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
            _window,
            SDL_DestroyWindow
    );

    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
            _renderer,
            SDL_DestroyRenderer
    );

    log( "window and renderer created" );

    /**
     * prepare font cache.
     * see FC2_TEAM_DRAW_TYPE_TEXT case about this struct and about font caching.
     *
     * the initial release handled this with raw pointers. it could have been avoided by using unique_ptr.
     * this should properly handle the fonts being closed and the pointer going through a proper deleter.
     */
    struct cache_destruction
    {
        auto operator( )( TTF_Font * font ) const -> void
        {
            if( !font ) return;
            TTF_CloseFont( font );
        }
    };
    typedef std::unique_ptr< TTF_Font, cache_destruction > font_cache_information;
    std::unordered_map< int, font_cache_information > fonts_cache;

    /**
     * rendering
     */
    SDL_Event event;
    while (true)
    {
        SDL_PollEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
        {
            break;
        }

        /**
         * get fc2 drawing requests
         *
         * if fc2 returns anything besides FC2_TEAM_ERROR_NO_ERROR that means
         * the solution is probably closed. therefore, we will automatically
         * close this too.
         */
        const auto drawing = fc2::draw::get();
        if( fc2::get_error() != FC2_TEAM_ERROR_NO_ERROR )
        {
            log( "solution appears to have closed" );
            break;
        }

        /**
         * handle requests now
         */
        const auto instance = renderer.get();

        SDL_SetRenderDrawColor(instance, 0, 0, 0, 0 );
        SDL_RenderClear(instance);
        for( const auto & [text, dimensions, style] : drawing )
        {

            /**
             * whatever we're drawing here, set the color beforehand.
             * also convert our dimensions to floating numbers
             */
            SDL_SetRenderDrawColor(
                instance,
                style[ FC2_TEAM_DRAW_STYLE_RED ],
                style[ FC2_TEAM_DRAW_STYLE_GREEN ],
                style[ FC2_TEAM_DRAW_STYLE_BLUE ],
                style[ FC2_TEAM_DRAW_STYLE_ALPHA ]
            );

            const std::array< float, 4 > dimensions_f =
            {
                static_cast< float >( dimensions[ FC2_TEAM_DRAW_DIMENSIONS_LEFT ] ),
                static_cast< float >( dimensions[ FC2_TEAM_DRAW_DIMENSIONS_TOP ] ),
                static_cast< float >( dimensions[ FC2_TEAM_DRAW_DIMENSIONS_RIGHT ] ),
                static_cast< float >( dimensions[ FC2_TEAM_DRAW_DIMENSIONS_BOTTOM ] )
            };

            switch( style[ FC2_TEAM_DRAW_STYLE_TYPE ] )
            {
                case FC2_TEAM_DRAW_TYPE_BOX:
                {
                    const SDL_FRect rect = { dimensions_f[ 0 ], dimensions_f[ 1 ], dimensions_f[ 2 ], dimensions_f[ 3 ] };
                    SDL_RenderRect(
                            instance,
                            & rect
                    );
                    break;
                }

                case FC2_TEAM_DRAW_TYPE_BOX_FILLED:
                {
                    const SDL_FRect rect = { dimensions_f[ 0 ], dimensions_f[ 1 ], dimensions_f[ 2 ], dimensions_f[ 3 ] };
                    SDL_RenderFillRect(
                            instance,
                            & rect
                    );
                    break;
                }

                case FC2_TEAM_DRAW_TYPE_LINE:
                {
                    SDL_RenderLine(
                            instance,
                            dimensions_f[ 0 ],
                            dimensions_f[ 1 ],
                            dimensions_f[ 2 ],
                            dimensions_f[ 3 ]
                    );
                    break;
                }

                case FC2_TEAM_DRAW_TYPE_TEXT:
                {
                    /**
                     * there are about 3 ways to approach this considering multiple font sizes need to be considered:
                     *      - openfont/closefont per frame/tick, which I consider to be bad/slow. can be avoided. this will create disk i/o impact and repeating loading and destroying fonts can degrade performance. too much overhead. it's going to be terrible.
                     *      - scale the text through a dynamic setting, but this makes everything inconsistent. there is no function in fc2 that gives the current rendering queue. even if i made an fc2 lua function for this, it will add an extra operation.
                     *      - cached fonts might seem like the best approach here. one-time disk access and consistent. especially if numerous scripts are rendering text with the same font size.
                     *
                     * the cached font script approach performs the best after some tests. the default font is 185.4kb,
                     * this should be fine if multiple fonts are cached. it will maximize performance, but memory usage may
                     * become a potential problem. most FC2 scripts use around the same font sizes.
                     */
                    TTF_Font * font = nullptr;
                    {
                        if( const auto font_size = style[ FC2_TEAM_DRAW_STYLE_FONT_SIZE ]; !fonts_cache.contains( font_size ) )
                        {
                            font = TTF_OpenFont( font_path.c_str(), static_cast< float >( font_size ) );
                            if( !font )
                            {
                                log( "{} could not be created at size {}", font_path, font_size );
                                break;
                            }

                            fonts_cache[ font_size ] = std::unique_ptr< TTF_Font, cache_destruction >( font );
                            log( "font {}:{} created", font_path, font_size );
                        }
                        else
                        {
                            font = fonts_cache[ font_size ].get();
                        }
                    }

                    const auto surface = TTF_RenderText_Solid(
                            font,
                            text,
                            strlen( text ),
                            SDL_Color(
                                    style[ FC2_TEAM_DRAW_STYLE_RED ],
                                    style[ FC2_TEAM_DRAW_STYLE_GREEN ],
                                    style[ FC2_TEAM_DRAW_STYLE_BLUE ],
                                    style[ FC2_TEAM_DRAW_STYLE_ALPHA ]
                            )
                    );
                    if( !surface )
                    {
                        log( "{} surface could not be created in this frame.\n", font_path );
                        break;
                    }

                    const SDL_FRect rect = { dimensions_f[ 0 ], dimensions_f[ 1 ], static_cast< float >( surface->w ), static_cast< float >( surface->h ) };

                    const auto texture = SDL_CreateTextureFromSurface(
                            instance,
                            surface
                    );
                    if( !texture )
                    {
                        log( "{} texture could not be created in this frame.\n", font_path );
                        break;
                    }

                    SDL_DestroySurface( surface );
                    SDL_RenderTexture(
                        instance,
                        texture,
                        nullptr,
                        &rect
                    );
                    SDL_DestroyTexture( texture );
                    break;
                }

                case FC2_TEAM_DRAW_TYPE_CIRCLE:
                {
                    const float center_x = ( dimensions_f[ 0 ] + dimensions_f[ 2 ] ) / 2.0f;
                    const float center_y = ( dimensions_f[ 1 ] + dimensions_f[ 3 ] ) / 2.0f;
                    const float radius = dimensions_f[ 2 ] / 2.0f;

                    constexpr int segments = 100;
                    for( int i = 0; i < segments; ++i )
                    {
                        const float t1 = ( 2.0f * M_PI * i ) / segments;
                        const float t2 = ( 2.0f * M_PI * ( i + 1 ) ) / segments;

                        const float x1 = center_x + radius * std::cos( t1 );
                        const float y1 = center_y + radius * std::sin( t1 );
                        const float x2 = center_x + radius * std::cos( t2 );
                        const float y2 = center_y + radius * std::sin( t2 );

                        SDL_RenderLine(
                            instance,
                            x1,
                            y1,
                            x2,
                            y2
                        );
                    }
                    break;
                }

                case FC2_TEAM_DRAW_TYPE_CIRCLE_FILLED:
                {
                    const float center_x = ( dimensions_f[ 0 ] + dimensions_f[ 2 ] ) / 2.0f;
                    const float center_y = ( dimensions_f[ 1 ] + dimensions_f[ 3 ] ) / 2.0f;
                    const float radius = dimensions_f[ 2 ] / 2.0f;

                    for( float w = 0; w < radius * 2; w ++ )
                    {
                        for( float h = 0; h < radius * 2; h ++ )
                        {
                            const float dx = radius - w;
                            const float dy = radius - h;
                            if( ( dx * dx + dy * dy ) <= ( radius * radius ) )
                            {
                                SDL_RenderPoint(
                                    instance,
                                    center_x + dx,
                                    center_y + dy
                                );
                            }
                        }
                    }
                    break;
                }

                default:
                case FC2_TEAM_DRAW_TYPE_NONE: break;
            }
        }

        SDL_RenderPresent(instance);
    }

    /**
     * exit
     */
    parent.reset();
    window.reset();
    renderer.reset();
    fonts_cache.clear();

    TTF_Quit();
    SDL_Quit();

    log( "closed" );
    return 0;
}
