#pragma once
#ifndef CATA_SRC_UI_MANAGER_H
#define CATA_SRC_UI_MANAGER_H

#include <functional>

#include "cuboid_rectangle.h"
#include "point.h"

namespace catacurses
{
class window;
} // namespace catacurses

/**
 * Adaptor between UI code and the UI management system. Using this class allows
 * UIs to be correctly resized and redrawn when the game window is resized or
 * when exiting from other UIs.
 *
 * Usage example:
 * ```
 *     // Effective in the local scope
 *     ui_adaptor ui;
 *     // Ncurses window for drawing
 *     catacurses::window win;
 *     // Things to do when the game window changes size
 *     ui.on_screen_resize( [&]( ui_adaptor & ui ) {
 *         // Create an ncurses window
 *         win = catacurses::newwin( TERMX / 2, TERMY / 2, point( TERMX / 4, TERMY / 4 ) );
 *         // The window passed to this call must contain all the space the redraw
 *         // callback draws to, to ensure proper refreshing when resizing or exiting
 *         // from other UIs.
 *         ui.position_from_window( win );
 *     } );
 *     // Mark the resize callback to be called on the first redraw
 *     ui.mark_resize();
 *     // Things to do when redrawing the UI
 *     ui.on_redraw( [&]( const ui_adaptor & ) {
 *         // Clear UI area
 *         werase( win );
 *         // Print things
 *         mvwprintw( win, point_zero, "Hello World!" );
 *         // Write to frame buffer
 *         wnoutrefresh( win );
 *     } );
 *
 *     input_context ctxt( "<CATEGORY_NAME>" );
 *     ctxt.register_action( "QUIT" );
 *     while( true ) {
 *         // Invalidate the top UI (that is, this UI) and redraw all
 *         // invalidated UIs (including lower UIs that calls this UI).
 *         // May call the resize callbacks.
 *         ui_manager::redraw();
 *         // Get user input. Note that this may call the resize and redraw
 *         // callbacks multiple times due to screen resize, rendering target
 *         // reset, etc.
 *         const std::string action = ctxt.handle_input();
 *         if( action == "QUIT" ) {
 *             break;
 *         }
 *     }
 * ```
 **/
class ui_adaptor
{
    public:
        using redraw_callback_t = std::function<void( const ui_adaptor & )>;
        using screen_resize_callback_t = std::function<void( ui_adaptor & )>;

        struct disable_uis_below {
        };

        /**
         * Construct a `ui_adaptor` which is automatically added to the UI stack,
         * and removed from the stack when it is deconstructed. (When declared as
         * a local variable, it is removed from the stack when leaving the local scope.)
         **/
        ui_adaptor();
        /**
         * `ui_adaptor` constructed this way will block any UIs below from being
         * redrawn or resized until it is deconstructed. It is used for `debug_msg`.
         **/
        explicit ui_adaptor( disable_uis_below );
        ui_adaptor( const ui_adaptor &rhs ) = delete;
        ui_adaptor( ui_adaptor &&rhs ) = delete;
        ~ui_adaptor();

        ui_adaptor &operator=( const ui_adaptor &rhs ) = delete;
        ui_adaptor &operator=( ui_adaptor &&rhs ) = delete;

        /**
         * Set the position and size of the UI to that of `win`. This information
         * is used to calculate which UIs need redrawing during resizing and when
         * exiting from other UIs, so do call this function in the resizing
         * callback and ensure `win` contains all the space you will be drawing
         * to. Transparency is not supported. If `win` is null, the function has
         * the same effect as `position( point_zero, point_zero )`
         **/
        void position_from_window( const catacurses::window &win );
        /**
         * Set the position and size of the UI to that of an imaginary
         * `catacurses::window` in normal font, except that the size can be zero.
         * Note that `topleft` and `size` are in console cells on both tiles
         * and curses builds.
         **/
        void position( const point &topleft, const point &size );
        /**
         * Set redraw and resize callbacks. The resize callback should
         * call `position` or `position_from_window` to set the size of the UI,
         * and (re-)calculate any UI data that is related to the screen size,
         * including `catacurses::window` instances. In most cases, you should
         * also call `mark_resize` along with `on_screen_resize` so the UI is
         * initialized by the resizing callback when redrawn for the first time.
         *
         * The redraw callback should only redraw to the area specified by the
         * `position` or `position_from_window` call. Content drawn outside this
         * area may not render correctly when resizing or exiting from other UIs.
         * Transparency is not currently supported.
         *
         * These callbacks should NOT:
         * - Construct new `ui_adaptor` instances
         * - Deconstruct old `ui_adaptor` instances
         * - Call `redraw` or `screen_resized`
         * - (Redraw callback) call `position_from_window`
         * - Call any function that does these things, except for `debugmsg`
         *
         * Otherwise, display glitches or even crashes might happen.
         *
         * Calling `debugmsg` inside the callbacks is (semi-)supported, but may
         * cause display glitches after the debug message is closed.
         **/
        void on_redraw( const redraw_callback_t &fun );
        /* See `on_redraw`. */
        void on_screen_resize( const screen_resize_callback_t &fun );

        /**
         * Mark this `ui_adaptor` for resizing the next time it is redrawn.
         * This is normally called alongside `on_screen_resize` to initialize
         * the UI on the first redraw. You should also use this function to
         * explicitly request a reinitialization if any value the screen resize
         * callback depends on (apart from the screen size) has changed.
         **/
        void mark_resize() const;

        /**
         * Invalidate this UI so it gets redrawn the next redraw unless an upper
         * UI completely occludes this UI. May also cause upper UIs to redraw.
         * Can be used to mark lower UIs for redrawing when their associated data
         * has changed.
         **/
        void invalidate_ui() const;

        /**
         * Reset all callbacks and dimensions. Will cause invalidation of the
         * previously specified screen area.
         **/
        void reset();

        /* See the `ui_manager` namespace */
        static void invalidate( const rectangle<point> &rect, bool reenable_uis_below );
        static void redraw();
        static void redraw_invalidated();
        static void screen_resized();
    private:
        static void invalidation_consistency_and_optimization();

        // pixel dimensions in tiles, console cell dimensions in curses
        rectangle<point> dimensions;
        redraw_callback_t redraw_cb;
        screen_resize_callback_t screen_resized_cb;

        bool disabling_uis_below;

        mutable bool invalidated;
        mutable bool deferred_resize;
};

/**
 * Helper class that fills the background and obscures all UIs below. It stays
 * on the UI stack until its lifetime ends.
 **/
class background_pane
{
    public:
        background_pane();
    private:
        ui_adaptor ui;
};

// export static funcs of ui_adaptor with a more coherent scope name
namespace ui_manager
{
/**
 * Invalidate a portion of the screen when a UI is resized, closed, etc.
 * Not supposed to be directly called by the user.
 * rect is the pixel dimensions in tiles or console cell dimensions in curses
 **/
void invalidate( const rectangle<point> &rect, bool reenable_uis_below );
/**
 * Invalidate the top window and redraw all invalidated windows.
 * Note that `ui_manager` may redraw multiple times when the game window is
 * resized or the system requests a redraw during input calls, so any data
 * that may change after a resize or on each redraw should be calculated within
 * the respective callbacks.
 **/
void redraw();
/**
 * Redraw all invalidated windows without invalidating the top window.
 **/
void redraw_invalidated();
/**
 * Handle resize of the game window.
 * Not supposed to be directly called by the user.
 **/
void screen_resized();
} // namespace ui_manager

#endif // CATA_SRC_UI_MANAGER_H
