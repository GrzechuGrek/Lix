/*
 * gameplay/gamepl_a.cpp
 *
 * Dinge, die bei einem aktiven Tick des Gameplays kontrolliert werden.
 * "Aktiv" heisst hierbei, dass kein Replay abgespielt wird, sondern spiel-
 * mechanisch relevante Dinge der Steuerung bearbeitet werden.
 *
 */

#include "gameplay.h"
#include "../other/user.h"

Replay::Data Gameplay::new_replay_data()
{
    Replay::Data data;
    data.player = malo->number;
    data.update = cs.update + 1;
    return data;
}

void Gameplay::calc_active()
{
    if (!malo) return;

    // Wo ist die Maus auf dem Land?
    int mx = map.get_mouse_x();
    int my = map.get_mouse_y();

    const bool mouse_on_panel = hardware.get_my() > map.get_screen_yl();

    // Remedies the bug: displays "N nothings" after level restart
    pan.stats.set_tarcnt(0);

    // Panels ueberpruefen. Zuerst Singleplayer-Panels.
    if (cs.tribes.size() == 1) {
        // Plus und Minus werden nicht auf "clicked" geprueft, sondern aktiv,
        // wenn gehalten. Dennoch benutzen wir Button::clicked(), um Halten
        // zu ermoeglichen.
        // Doppelklicks auf F1 oder F2 werden leider nicht erkannt, im Gegen-
        // satz zu Doppelklicks auf F12 fuer die Nuke. Das liegt daran, dass
        // die Tasten nach etwas gedrueckt gehaltener Zeit immer wieder
        // key_once() ausloesen und es somit auch beim Gedrueckthalten unbe-
        // absichtigt zur Max-/Min-Einstellung kaeme.
        // Dies aendert nur die angezeigte Zahl. Die Ratenaenderung wird erst,
        // wenn ein Update ansteht, zum Replay geschickt. Das spart Ballast,
        // da alle bis auf die letzte Aenderung pro Update egal sind.
        // Modulo 2 rechnen wir bei den Help::timer_ticks, weil Frank die
        // Aenderung der Rate auch bei 60 /sec zu rasant war.
        bool minus_clicked = pan.rate_minus.get_clicked()
                          || hardware.key_release(useR->key_rate_minus);
        bool plus_clicked  = pan.rate_plus .get_clicked()
                          || hardware.key_release(useR->key_rate_plus);
        if (hardware.key_hold(useR->key_rate_minus)) pan.rate_minus.set_down();
        if (hardware.key_hold(useR->key_rate_plus))  pan.rate_plus.set_down();
        if (pan.rate_minus.get_down() || minus_clicked) {
            // Doppelklick?
            if (minus_clicked && Help::timer_ticks - timer_tick_last_F1
             <= hardware.doubleclick_speed) {
                pan.rate_cur.set_number(trlo->spawnint_slow);
            }
            else if (minus_clicked) timer_tick_last_F1 = Help::timer_ticks;
            else {
                // Normales Halten
                if (pan.rate_cur.get_number() < trlo->spawnint_slow) {
                    if (Help::timer_ticks % 3 == 0)
                     pan.rate_cur.set_number(pan.rate_cur.get_number() + 1);
                }
                else pan.rate_minus.set_down(false);
            }
        }
        // Plus
        if (pan.rate_plus.get_down() || plus_clicked) {
            if (plus_clicked && Help::timer_ticks - timer_tick_last_F2
             <= hardware.doubleclick_speed) {
                pan.rate_cur.set_number(trlo->spawnint_fast);
            }
            else if (plus_clicked) timer_tick_last_F2 = Help::timer_ticks;
            else {
                if (pan.rate_cur.get_number() > trlo->spawnint_fast) {
                    if (Help::timer_ticks % 3 == 0)
                     pan.rate_cur.set_number(pan.rate_cur.get_number() - 1);
                }
                else pan.rate_plus.set_down(false);
            }
        }
    }

    // Check skill buttons
    if (malo) {
        // This starts at the next button and breaks immediately if one is
        // pressed. This enables a hotkey to cycle through its skills.
        // We don't read malo->skill_sel, this is only updated after a
        // gameplay physics update.
        // This next if makes that an overloaded hotkey will select the skill
        // that's more to the left always, unless a skill with the same
        // hotkey is already selected. If the if wasn't there, sometimes the
        // first hit of a new skillkey would select a different than the
        // leftmost occurence, based on where the skills are.
        GameplayPanel::SkBIt cur_but
            = pan.button_by_replay_id(malo->skill_sel);
        int pan_vec_id = cur_but - pan.skill.begin();
        if (cur_but == pan.skill.end() || ! cur_but->get_clicked())
            pan_vec_id = pan.skill.size() - 1;
        // we don't need cur_but from here on anymore, only the panel ID

        // Scan for hotkey presses of empty/nonpresent skills
        // if this is still false later, iterate over nonpresent skills
        // and play the empty-skill sound if a key for them was pressed
        bool some_panel_action = false;

        for (size_t j = 0; j < pan.skill.size(); ++j) {
            size_t i = (pan_vec_id + j + 1) % pan.skill.size();
            if (pan.skill[i].get_number() != 0
             && pan.skill[i].get_clicked()
             && ! pan.skill[i].get_on())
            {
                int rep_id = pan.skill[i].get_replay_id();

                // This will be done during the next physics update, but
                // we'll do it now, to make the button seem more responsive
                pan.set_skill_on(rep_id);
                // Das hier ist das eigentlich Wichtige
                Replay::Data data = new_replay_data();
                data.action       = Replay::SKILL;
                data.what         = rep_id;
                replay.add(data);
                Network::send_replay_data(data);
                // Jetzt schon den Klang abspielen, dafuer beim Update nicht.
                // Es wird also der Effekt gesichert und zusaetzlich manuell
                // der Effekt vorgemerkt, falls jemand in der Pause wechselt.
                effect.add_sound(cs.update + 1, *trlo, rep_id, Sound::PANEL);
                Sound::play_loud(Sound::PANEL);
                // Don't check any more buttons, see comment before the loop.
                some_panel_action = true;
                break;
            }
        }

        if (! some_panel_action) {
            // check for empty clicked panel icons
            for (size_t i = 0; i < pan.skill.size(); ++i)
             if (pan.skill[i].get_clicked()
             &&  pan.skill[i].get_skill() != LixEn::NOTHING) {
                if (! pan.skill[i].get_on()) {
                    if (hardware.get_ml()) {
                        Sound::play_loud(Sound::PANEL_EMPTY);
                    }
                    // else play no sound -- we're holding the mouse button
                    // over a wrong skill, that's not notify-necessary, but
                    // leave open the possibility to play a sound later
                }
                // we've clicked on an activated skill, that's all good,
                // never play a sound even if a hotkey was used
                else some_panel_action = true;
            }
            // check for hotkeys of present/nonpresent skills:
            // if the hotkey is of an available skill, we wouldn't have ended
            // up in this if (! some_panel_action)
            if (! some_panel_action)
             for (size_t i = 0; i < LixEn::AC_MAX; ++i) {
                const int key = useR->key_skill[i];
                if (key != 0 && hardware.key_once(key))
                 Sound::play_loud(Sound::PANEL_EMPTY);
            }

        }
    }
    // Restliche Buttons in der normalen Calculate-Funktion

    // Atombombe
    if (pan.get_nuke_doubleclicked()) {
        // set_on() kommt zwar auch im Update, aber wenn wir das
        // hier immer machen, sieht es besser aus. Gleiches gilt fuer
        // den Sound, ist wie beim normalen Anklicken.
        pan.nuke_single.set_on();
        pan.nuke_multi .set_on();
        pan.pause      .set_off();
        Replay::Data data = new_replay_data();
        data.action       = Replay::NUKE;
        replay.add(data);
        Network::send_replay_data(data);
        effect.add_sound(cs.update + 1, *trlo, 0, Sound::NUKE);
        Sound::play_loud(Sound::NUKE);
    }



    ///////////////////////////////////////
    // End of: check things in the panel //
    // Now: mouse on the playing field   //
    ///////////////////////////////////////



    // mouse on the playing field, lixes are selectable
    if (! mouse_on_panel && trlo) {
        // Bestimmte Richtung anw�hlen?
        bool only_dir_l = false;
        bool only_dir_r = false;
        if (  key[useR->key_force_left]
         && ! key[useR->key_force_right]) {
            only_dir_l = true;
            mouse_cursor.set_x_frame(1);
        }
        else if (! key[useR->key_force_left]
         &&        key[useR->key_force_right]) {
            only_dir_r = true;
            mouse_cursor.set_x_frame(2);
        }
        // Decide which lix the cursor points at
        // Die Liste der Lixen wird durchlaufen und die Priorit�t jeder
        // Lix errechnet. Wird eine h�here Priorit�t als die derzeitig
        // h�chste gefunden, wechselt LixIt target. Bei gleicher Prioritaet
        // haben Lixen, die naeher am Mauscursor liegen, Vorrang! Mit rechter
        // Maustaste (selectable in the options) waehlt man dagegen die letzte
        // Lix mit der niedrigsten Priorit�t. Auch hier haben naeher liegende
        // Lixen Vorrang.
        LixIt  target = trlo->lixvec.end(); // Klickbar mit Prioritaet
        LixIt  tarinf = trlo->lixvec.end(); // Nicht unb. klickbar mit Prior.
        int    tarcnt = 0; // Anzahl Lixen unter Cursor
        int    target_priority = 0;
        int    target_prio_min = 100000; // if (< target_prio) => tooltip
        int    tarinf_priority = 0;
        double target_hypot = 1000;
        double tarinf_hypot = 1000;
        bool   tooltip_l_elig = false; // if both true => tooltip
        bool   tooltip_r_elig = false; // is there a l/r walker considered?

        // Bei Zoom diese Variablen veraendern
        int zoom   = map.get_zoom();
        int mmld_x = mouse_max_lix_distance_x - mouse_cursor_offset/2*zoom;
        int mmld_u = mouse_max_lix_distance_u - mouse_cursor_offset/2*zoom;
        int mmld_d = mouse_max_lix_distance_d - mouse_cursor_offset/2*zoom;

        // trlo->skill_sel is only set during next update to what's clicked.
        // Therefore, look manually through the panel to see what has been
        // clicked most recently.
        GameplayPanel::SkBIt skill_visible = pan.skill.begin();
        while (skill_visible != pan.skill.end()
            && (! skill_visible->get_on()
                // panel has reordered skills. trlo and the replay don't
                || trlo->skill[skill_visible->get_replay_id()].nr == 0
                || skill_visible->get_number() == 0))
            ++skill_visible;

        // this is only for emergencies, to make the cursor open properly
        if (skill_visible == pan.skill.end())
            skill_visible = pan.button_by_replay_id(malo->skill_sel);

        if (skill_visible != pan.skill.end())
            for (LixIt i =  --trlo->lixvec.end();
                       i != --trlo->lixvec.begin(); --i)
        {
            if (map.distance_x(i->get_ex(), mx) <=  mmld_x
             && map.distance_x(i->get_ex(), mx) >= -mmld_x
             && map.distance_y(i->get_ey(), my) <=  mmld_d
             && map.distance_y(i->get_ey(), my) >= -mmld_u)
            {
                // Hypot geht von (ex|ey+etwas) aus
                // true = Beachte persoenliche Einschraenkungen wie !MultBuild
                int priority = i->get_priority(
                    trlo->skill[skill_visible->get_replay_id()].ac, true);

                // Invert priority if a corresponding mouse button is held
                if ((hardware.get_mrh() && useR->prioinv_right)
                 || (hardware.get_mmh() && useR->prioinv_middle)
                 ||  hardware.key_hold(useR->key_priority)) {
                    priority = 100000 - priority;
                }
                double hypot = map.hypot(mx, my, i->get_ex(),
                                          i->get_ey() + ((mmld_d - mmld_u)/2)
                                          );
                if (priority > 0 && priority < 100000) {
// This is horrible code. 9 indentation levels imply a severe problem,
// we should use more functions. Will fix this in the D port.

// Die Anforderungen den offenen Mauscursur
// und das Schreiben des Strings auf die Info...
++tarcnt;
if (priority >  tarinf_priority
 ||(priority == tarinf_priority && hypot < tarinf_hypot)) {
    tarinf = i;
    tarinf_priority = priority;
    tarinf_hypot    = hypot;
}
// ...sind geringer als die f�r Anklick-Inbetrachtnahme!
if (priority > 1 && priority < 99999) {
    if (!(only_dir_l && i->get_dir() ==  1)
     && !(only_dir_r && i->get_dir() == -1)) {
        // consider this clickable and eligible for tooltip
        if  (priority >  target_priority
         || (priority == target_priority && hypot < target_hypot)) {
            if (target_priority != 0)
                pan.suggest_tooltip_priority();
            target          = i;
            target_priority = priority;
            target_hypot    = hypot;
            if (priority < target_prio_min)
                target_prio_min = priority;
        }
        else if (priority <  target_prio_min
             || (priority == target_prio_min && hypot >= target_hypot)) {
            if (target_prio_min != 100000)
                pan.suggest_tooltip_priority();
            target_prio_min = priority;
        }
    }
    if (i->get_dir() ==  1) tooltip_r_elig = true;
    if (i->get_dir() == -1) tooltip_l_elig = true;
    if (tooltip_r_elig && tooltip_l_elig)
        pan.suggest_tooltip_force_dir();
}
                }
            }
        }

        // Auswertung von tarinf
        if (tarinf != trlo->lixvec.end()) {
            mouse_cursor.set_y_frame(1);
        }
        pan.stats.set_tarinf(tarinf == trlo->lixvec.end() ? 0 : &*tarinf);
        pan.stats.set_tarcnt(tarcnt);

        // tooltips for queuing builders/platformers
        if (target != trlo->lixvec.end()
         && skill_visible->get_number() != 0) {
            if (target->get_ac() == LixEn::BUILDER
             && skill_visible->get_skill() == LixEn::BUILDER)
                pan.suggest_tooltip_builders();
            else if (target->get_ac() == LixEn::PLATFORMER
             && skill_visible->get_skill() == LixEn::PLATFORMER)
                pan.suggest_tooltip_platformers();
        }

        // Resolving target
        // target == trlo->lixvec.end() if there was nobody under the cursor,
        // or if skill_visible is somehow pan.skill.end(), shouldn't happen
        // we're also checking the displayed number, look at comment about the
        // visible number due to eye candy/making button seem more responsive
        if (target != trlo->lixvec.end() && hardware.get_ml()) {
            if (skill_visible->get_number() != 0) {
                // assign
                const int lem_id = target - trlo->lixvec.begin();
                pan.pause.set_off();

                Replay::Data data = new_replay_data();
                data.action       = only_dir_l ? Replay::ASSIGN_LEFT
                                  : only_dir_r ? Replay::ASSIGN_RIGHT
                                  : Replay::ASSIGN;
                data.what         = lem_id;
                replay.add(data);
                Network::send_replay_data(data);

                // Die sichtbare Zahl hinabsetzen geschieht nur fuer's Auge,
                // in Wirklichkeit geschieht dies erst beim Update. Das Augen-
                // spielzeug verabreichen wir allerdings nur, wenn nicht z.B.
                // zweimal auf dieselbe Lix mit derselben Faehigkeit
                // geklickt wurde. Den unwahrscheinlichen Fall, dass man
                // zweimal mit beide Male anwendbaren Faehigkeiten auf dieselbe
                // Lix geklickt hat, koennen wir vernachlaessigen - dann
                // erscheint die Nummernaenderung eben erst beim kommenden Upd.
                // Auch der Sound (s.u.) wird dann nicht gespielt.
                if (!replay.get_on_update_lix_clicked(cs.update + 1, lem_id)
                 && skill_visible->get_number() != LixEn::infinity) {
                    skill_visible->set_number(skill_visible->get_number() - 1);
                }
                // Sound in der Effektliste speichern, damit er nicht beim Upd.
                // nochmal ertoent, und dazu wird er hier nochmals gespielt,
                // damit wir sicher gehen, dass er beim Klick kommt.
                Sound::Id snd = Lixxie::get_ac_func(skill_visible
                                ->get_skill()).sound_assign;
                effect.add_sound(cs.update + 1, *trlo, lem_id, snd);
                Sound::play_loud(snd);
            }
            else {
                Sound::play_loud(Sound::PANEL_EMPTY);
            }
        }

    }
    // end of: mouse in the playing field, no aiming

}
