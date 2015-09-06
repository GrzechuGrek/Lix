/*
 * lix/walker.cpp
 *
 * Der Walker nutzt auch viel den Ascender zum Erklimmen von hoehren Stufen,
 * der entsprechende Code steht in climber.cpp.
 *
 */

#include "ac.h"

void assclk_walker(Lixxie& l)
{
    if (l.get_ac() == LixEn::WALKER
     || l.get_ac() == LixEn::RUNNER
     || l.get_ac() == LixEn::LANDER) {
        l.turn();
        // Da bei Walker -> Walker nicht --frame von evaluate_click() kommt,
        // setzen wir hier manuell das Frame auf -1, weil wir das 0. wollen.
        if (l.get_ac() == LixEn::WALKER) l.set_frame(-1);
        if (l.get_ac() == LixEn::RUNNER) l.set_frame(-1);
    }
    else if (l.get_ac() == LixEn::STUNNER
          || l.get_ac() == LixEn::ASCENDER) {
        // lix_ac.cpp only allows to get here when the frame is high enough
        l.become(LixEn::WALKER);
        l.turn();
    }
    else if (l.get_ac() == LixEn::BLOCKER) {
        // Da assign haeufig beim Mausklick-Zuweisen aufgerufen wird, gilt
        // wieder die Konvention, dass --frame hinterher gemacht wird, also:
        if (l.get_frame() < 20) l.set_frame(21);
        else                    l.turn(); // turn a blocker->walker transistion
    }
    else if (l.get_ac() == LixEn::PLATFORMER && l.get_frame() > 5) {
        l.set_ac(LixEn::SHRUGGER2);
        l.set_frame(9);
        // see also next else if. Clicking twice on the platformer shall turn
        // it around.
    }
    else if (l.get_ac() == LixEn::SHRUGGER || l.get_ac() == LixEn::SHRUGGER2) {
        l.become(LixEn::WALKER);
        l.turn();
    }
    else {
        l.become(LixEn::WALKER);
    }
}



void become_walker(Lixxie& l)
{
    if (l.get_runner()) l.become        (LixEn::RUNNER);
    else                l.become_default(LixEn::WALKER);
}



void update_walker_or_runner(Lixxie& l, const UpdateArgs& ua);

void update_walker(Lixxie& l, const UpdateArgs& ua)
{
    if (l.is_last_frame())  l.set_frame(3);
    else                    l.next_frame();

    update_walker_or_runner(l, ua);
}













static bool handle_wall_here(Lixxie& l)
{
    bool turn_after_all = false;

    // Pruefung auf Boden unter der neuen Position
    // Falls da nichts ist, gucken wir etwas dar�ber nach - vielleicht
    // handelt es sich um eine sehr d�nne Aufw�rts-Br�cke, durch die
    // wir nicht hindurchfallen wollen?
    if (l.is_solid() || l.is_solid(0, 1)) {
        // do the wall check to turn or ascend
        int up_by = l.solid_wall_height(0);
        if      (up_by == 13) turn_after_all = true;
        else if (up_by >=  6) l.become(LixEn::ASCENDER);
        else                  l.move_up(up_by);
    }

    return turn_after_all;
}



static bool handle_pit_here(Lixxie& l) {
    bool fall_down = false;

    // Kein Boden? Dann hinunter gehen oder zu fallen beginnen
    if (!l.is_solid() && !l.is_solid(0, 1)) {
        int moved_down_by = 0;
        for (int i = 3; i < 11; ++i) {
            if (!l.is_solid()) {
                l.move_down(1);
                ++moved_down_by;
            }
            else break;
        }
        if (l.is_solid()) {
            // Don't move that far back up as the check about 10 lines further
            // down that reads very similar in its block
            if (moved_down_by > 6) {
                l.move_up(4);
                l.become(LixEn::FALLER);
                l.set_special_x(moved_down_by - 4);
                fall_down = true;
            }
        }
        else {
            // Aber nicht direkt so weit nach unten zum Fallen bewegen
            l.move_up(6);
            l.become(LixEn::FALLER);
            l.set_special_x(2);
            fall_down = true;
        }
    }

    return fall_down;
}










void update_walker_or_runner(Lixxie& l, const UpdateArgs& ua)
{
    ua.suppress_unused_variable_warning();

    const int old_ex = l.get_ex();
    const int old_ey = l.get_ey();
    const Lookup::LoNr old_enc_foot = l.get_foot_encounters();
    const Lookup::LoNr old_enc_body = l.get_body_encounters();

    bool fall_down = handle_pit_here(l);

    if (fall_down) {
        return;
    }

    // Das erste Frame dient zur kurzen Pause, die die Lix vor dem
    // Weiterlaufen machen soll, wenn die Walker-Faehigkeit vom Benutzer
    // explizit zugewiesen wird. Dieses Frame darf im normalen Walker-
    // Framezyklus nicht angenommen werden.
    // Also true for runners.
    if (l.get_frame() != 0) l.move_ahead();

    bool turn_after_all = handle_wall_here(l);

    // Wenn die Lix umdrehen soll, beginnt sie entweder zu klettern
    // oder dreht um, beides auf ihrer alten Stelle
    if (turn_after_all) {
        l.set_ex(old_ex);
        l.set_ey(old_ey);
        l.set_foot_encounters(old_enc_foot);
        l.set_body_encounters(old_enc_body);
        bool climbed_after_all = false;

        if (l.get_climber()) {
            // Auf Landschaft �ber der derzeitigen Position pr�fen
            bool enough_space = true;
            for (int i = 1; i < 13; ++i) {
                if (l.is_solid(0, -i)) {
                    enough_space = false;
                    break;
                }
            }
            if (enough_space) {
                l.become(LixEn::CLIMBER);
                climbed_after_all = true;
            }
        }
        if (! climbed_after_all) {
            l.turn();
            // this new check will take care of the bugs around 2012-02,
            // the lix didn't ascend or fall when caught in 2-pixel gaps.
            bool in_wall = handle_wall_here(l);
            if (!in_wall) {
                handle_pit_here(l);
            }
        }
    }
    else {
        handle_pit_here(l);
    }
    // Ende der Umdrehen-Kontrolle
}
