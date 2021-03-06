/*
 * lix/lix.cpp
 *
 */

#include <algorithm> // swap
#include <cmath> // sin - for the fuse

#include "ac.h" // update args
#include "lix.h"

#include "../gameplay/effect.h"
#include "../graphic/gra_lib.h"
#include "../other/globals.h"   // Dateiname des Lixxie-Bilds
#include "../other/help.h"      // Help::mod

Torbit*        Lixxie::land       = 0;
Lookup*        Lixxie::lookup     = 0;
Map*           Lixxie::ground_map = 0;
EffectManager* Lixxie::effect     = 0;

bool           Lixxie::any_new_flingers = false;

const int Lixxie::distance_safe_fall   = 126;
const int Lixxie::distance_float       =  60;
const int Lixxie::updates_for_bomb     =  75;



// Dies wird hier nur standardkonstruiert, load_all_bitmaps() fuellt es auf.
Lixxie::Matrix Lixxie::countdown;


Lixxie::Lixxie(
    Tribe*     new_tribe,
    int        new_ex,
    int        new_ey
) :
    Graphic            (GraLib::get_lix(new_tribe ? new_tribe->style
                        : LixEn::GARDEN), *ground_map,
                        Help::even(new_ex) - LixEn::ex_offset,
                        new_ey             - LixEn::ey_offset),
    tribe              (new_tribe),
    marked             (0),
    dir                (1),
    special_x          (0),
    special_y          (0),
    queue              (0),

    fling_x            (0),
    fling_y            (0),
    fling_new          (false),
    fling_by_same_tribe(false),

    frame              (0),
    updates_since_bomb (0),
    exploder_knockback (false),
    runner             (false),
    climber            (false),
    floater            (false),
    enc_body           (0),
    enc_foot           (0),

    style              (tribe ? tribe->style : LixEn::GARDEN),
    ac                 (LixEn::NOTHING)
{
    if (tribe) {
        become(LixEn::FALLER);
        frame = 4;
    }
    // Wichtig fuer den Torus: Rechtzeitig Modulo rechnen
    set_ex(Help::even(new_ex));
    set_ey(new_ey);
}

Lixxie::~Lixxie()
{
}






void Lixxie::set_static_maps(Torbit& l, Lookup& lo, Map& g)
{
    land       = &l;
    lookup     = &lo;
    ground_map = &g;
}



inline static int frame_to_x_frame(int frame) { return frame + 2; }
inline static int ac_to_y_frame   (int ac)    { return ac - 1;    }

// These are used for encounters and to draw the fuse
inline static int get_fuse_x(const Lixxie& l)
{
    int ret = Lixxie::countdown[frame_to_x_frame(l.get_frame())]
                               [ac_to_y_frame   (l.get_ac())   ].x;
    if (l.get_dir() < 0) ret = GraLib::get_lix(l.get_style()).get_xl() - ret;
    ret += l.get_x();
    return ret;
}
inline static int get_fuse_y(const Lixxie& l)
{
    int ret = Lixxie::countdown[frame_to_x_frame(l.get_frame())]
                               [ac_to_y_frame   (l.get_ac())   ].y;
    ret += l.get_y();
    return ret;
}



void Lixxie::record_encounters()
{
    enc_foot |= lookup->get(ex, ey);
    enc_body |= enc_foot
             |  lookup->get(ex, ey - 4)
             |  lookup->get(get_fuse_x(*this), get_fuse_y(*this));
}

void Lixxie::set_ex(const int n) {
    ex = Help::even(n);
    set_x(ex - LixEn::ex_offset);
    if (ground_map->get_torus_x()) ex = Help::mod(ex, land->get_xl());
    record_encounters();
}

void Lixxie::set_ey(const int n) {
    ey = n;
    set_y(ey - LixEn::ey_offset);
    if (ground_map->get_torus_y()) ey = Help::mod(ey, land->get_yl());
    record_encounters();
}

void Lixxie::move_ahead(int plus_x)
{
    plus_x = Help::even(plus_x);
    plus_x *= dir;
    for ( ; plus_x > 0; plus_x -= 2) set_ex(ex + 2);
    for ( ; plus_x < 0; plus_x += 2) set_ex(ex - 2);
}

void Lixxie::move_down(int plus_y)
{
    for ( ; plus_y > 0; --plus_y) set_ey(ey + 1);
    for ( ; plus_y < 0; ++plus_y) set_ey(ey - 1);
}

void Lixxie::move_up(int minus_y)
{
    move_down(-minus_y);
}

void Lixxie::turn()
{
    dir *= -1;
}



bool Lixxie::get_in_trigger_area(const EdGraphic& gr) const
{
    const Object& ob = *gr.get_object();
    return ground_map->get_point_in_rectangle(
        get_ex(), get_ey(),
        gr.get_x() + ob.get_trigger_x(),
        gr.get_y() + ob.get_trigger_y(),
        ob.trigger_xl, ob.trigger_yl);
}



void Lixxie::add_fling(const int px, const int py, const bool same_tribe)
{
    if (fling_by_same_tribe && same_tribe) return;

    any_new_flingers    = true;
    fling_by_same_tribe = (fling_by_same_tribe || same_tribe);
    fling_new = true;
    fling_x   += px;
    fling_y   += py;
}

void Lixxie::reset_fling_new()
{
    any_new_flingers    = false;
    fling_new           = false;
    fling_by_same_tribe = false;
    fling_x             = 0;
    fling_y             = 0;
}



bool Lixxie::get_steel(const int px, const int py) {
    return lookup->get_steel(ex + px * dir, ey + py);
}

bool Lixxie::get_steel_absolute(const int x, const int y) {
    return lookup->get_steel(x, y);
}

void Lixxie::add_land(const int px, const int py, const AlCol col) {
    add_land_absolute(ex + px * dir, ey + py, col);
}

void Lixxie::add_land_absolute(const int x, const int y, const AlCol col) {
    land->set_pixel(x, y, col);
    lookup->add    (x, y, Lookup::bit_terrain);
}

bool Lixxie::is_solid(int px, int py) {
    return lookup->get_solid_even(ex + px * dir, ey + py);
}

bool Lixxie::is_solid_single(int px, int py) {
    return lookup->get_solid(ex + px * dir, ey + py);
}



int Lixxie::solid_wall_height(const int px, const int py)
{
    int solid = 0;
    for (int i = 1; i > -12; --i) {
        if (is_solid(px, py + i)) ++solid;
        else break;
    }
    return solid;
}



int Lixxie::count_solid(int x1, int y1, int x2, int y2)
{
    if (x2 < x1) std::swap(x1, x2);
    if (y2 < y1) std::swap(y1, y2);
    // Totaler R�ckgabewert
    int ret = 0;
    for (int ix = Help::even(x1); ix <= Help::even(x2); ix += 2) {
        for (int iy = y1; iy <= y2; ++iy) {
            if (is_solid(ix, iy)) ++ret;
        }
    }
    return ret;
}



int Lixxie::count_steel(int x1, int y1, int x2, int y2)
{
    if (x2 < x1) std::swap(x1, x2);
    if (y2 < y1) std::swap(y1, y2);
    // Totaler R�ckgabewert
    int ret = 0;
    for (int ix = Help::even(x1); ix <= Help::even(x2); ix += 2) {
        for (int iy = y1; iy <= y2; ++iy) {
            if (get_steel(ix, iy)) ++ret;
        }
    }
    return ret;
}

// ############################################################################
// ############################################################## Malfunktionen
// ############################################################################



bool Lixxie::remove_pixel(int px, int py)
{
    // Dies nur bei draw_pixel() und remove_pixel()
    if (dir < 0) --px;

    // Abbaubare Landschaft?
    if (! get_steel(px, py) && is_solid(px, py)) {
        lookup->rm     (ex + px * dir, ey + py, Lookup::bit_terrain);
        land->set_pixel(ex + px * dir, ey + py, color[COL_PINK]);
        return false;
    }
    // Stahl?
    else if (get_steel(px, py)) return true;
    else return false;
}



void Lixxie::remove_pixel_absolute(int x, int y)
{
    if (!get_steel_absolute(x, y) && lookup->get_solid(x, y)) {
        lookup->rm(x, y, Lookup::bit_terrain);
        land->set_pixel(x, y, color[COL_PINK]);
    }
}



bool Lixxie::remove_rectangle(int x1, int y1, int x2, int y2)
{
    if (x2 < x1) std::swap(x1, x2);
    if (y2 < y1) std::swap(y1, y2);
    // Totaler R�ckgabewert
    bool ret = false;
    for (int ix = x1; ix <= x2; ++ix) {
        for (int iy = y1; iy <= y2; ++iy) {
            // Ab einem Stahl ist der totale R�ckgabewert true
            if (remove_pixel(ix, iy)) ret = true;
        }
    }
    return ret;
}



// �hnlich wie remove_pixel...
void Lixxie::draw_pixel(int px, int py, int col)
{
    // Dies nur bei draw_pixel() und remove_pixel()
    if (dir < 0) --px;

    if (! is_solid_single(px, py)) add_land(px, py, col);
}



void Lixxie::draw_rectangle(int x1, int y1, int x2, int y2, int col)
{
    if (x2 < x1) std::swap(x1, x2);
    if (y2 < y1) std::swap(y1, y2);

    // Alle Pixel im Rechteck durchgehen
    for (int ix = x1; ix <= x2; ++ix) {
        for (int iy = y1; iy <= y2; ++iy) {
            draw_pixel(ix, iy, col);
        }
    }
}



void Lixxie::draw_brick(int x1, int y1, int x2, int y2)
{
    const int col_l = get_cutbit()->get_pixel(19, LixEn::BUILDER - 1, 0, 0);
    const int col_m = get_cutbit()->get_pixel(20, LixEn::BUILDER - 1, 0, 0);
    const int col_d = get_cutbit()->get_pixel(21, LixEn::BUILDER - 1, 0, 0);

    draw_rectangle(x1 + (dir<0), y1, x2 - (dir>0), y1, col_l);
    draw_rectangle(x1 + (dir>0), y2, x2 - (dir<0), y2, col_d);
    if (dir > 0) {
        draw_pixel(x2, y1, col_m);
        draw_pixel(x1, y2, col_m);
    }
    else {
        draw_pixel(x1, y1, col_m);
        draw_pixel(x2, y2, col_m);
    }
}

/*
 * Draws the the rectangle specified by xs, ys, ws, hs of the
 * specified animation frame onto the level map at position (xd, yd),
 * where (xd, yd) specifies the top left of the destination rectangle
 * relative to the lix' position
 */
void Lixxie::draw_frame_to_map
    (
        int frame, int anim,
        int xs, int ys, int ws, int hs,
        int xd, int yd
    )
{
    for (int y = 0; y < hs; ++y) {
        for (int x = 0; x < ws; ++x) {
            const AlCol col = get_cutbit()->get_pixel(frame, anim, xs+x, ys+y);
            if (col != color[COL_PINK] && ! get_steel(xd + x, yd + y)) {
                add_land(xd + x, yd + y, col);
            }
        }
    }
}



void Lixxie::play_sound(const UpdateArgs& ua, Sound::Id sound_id)
{
    if (effect) effect->add_sound(ua.st.update, *tribe, ua.id, sound_id);
}



void Lixxie::play_sound_if_trlo(const UpdateArgs& ua, Sound::Id sound_id)
{
    if (effect) effect->add_sound_if_trlo(
                            ua.st.update, *tribe, ua.id, sound_id);
}



bool Lixxie::is_last_frame()
{
    const Cutbit& c = *get_cutbit();
    BITMAP* b = c.get_al_bitmap();
    int pixel_col
     = getpixel(b, (frame + 3)*(c.get_xl()+1)+1,  (ac - 1)*(c.get_yl()+1)+2);
    if (frame == c.get_x_frames() - 3 || pixel_col == getpixel(b, b->w - 1, 0))
     return true;
    return false;
}



void Lixxie::next_frame(int loop)
{
    if (is_last_frame() || frame + 3 == loop) frame = 0;
    else frame++;
}



void Lixxie::draw()
{
    if (ac == LixEn::NOTHING) return;

    set_x_frame(frame_to_x_frame(frame));
    set_y_frame(ac_to_y_frame(ac));

    // Wenn ein Z�hlwerk erforderlich ist...
    if (updates_since_bomb > 0) {
        int fuse_x = get_fuse_x(*this);
        int fuse_y = get_fuse_y(*this);

        // Hierhin zeichnen
        Torbit& tb = *ground_map;

        int x = 0;
        int y = 0;
        for (; -y < (updates_for_bomb - (int) updates_since_bomb)/5+1; --y) {
            const int u = updates_since_bomb;
            x           = (int) (std::sin(u/2.0) * 0.02 * (y-4) * (y-4));
            tb.set_pixel(fuse_x + x-1, fuse_y + y-1, color[COL_GREY_FUSE_L]);
            tb.set_pixel(fuse_x + x-1, fuse_y + y  , color[COL_GREY_FUSE_L]);
            tb.set_pixel(fuse_x + x  , fuse_y + y-1, color[COL_GREY_FUSE_D]);
            tb.set_pixel(fuse_x + x  , fuse_y + y  , color[COL_GREY_FUSE_D]);
        }
        // Flamme zeichnen
        const Cutbit& cb = GraLib::get(gloB->file_bitmap_fuse_flame);
        cb.draw(*ground_map,
         fuse_x + x - cb.get_xl()/2,
         fuse_y + y - cb.get_yl()/2,
         updates_since_bomb % cb.get_x_frames(), 0);
    }
    // Zuendschnur fertig

    // mirror kippt vertikal, also muss bei dir < 0 auch noch um 180 Grad
    // gedreht werden. Allegro-Zeichenfunktionen bieten oft ebenfalls nur
    // vertikale Kippung, ich benutze daher ebenfalls diese Konvention.
    set_mirror  (     dir < 0 );
    set_rotation(2 * (dir < 0));
    Graphic::draw();
}
