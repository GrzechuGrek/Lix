/*
 * gameplay/tribe.h
 *
 * Informationen ueber einen Spieler: Eine Liste mit der eigenen
 * Lixhorde, die ausgewaehlte Farbe, Lixen in der Klappe, im Ziel usw.
 *
 * Die Klasse Gameplay verwaltet die korrekte Anzahl Spieler in entsprechend
 * vielen Player-Objekten. Eines dieser Objekte ist als lokaler Spieler
 * markiert, ihm kommen alle Klicks usw. zugute.
 *
 * char position
 *
 *   Nicht Spielernummer, sondern Startplatz. Wird im Netzwerk durch eine
 *   zufaellige Permutation ermittelt.
 *
 * bool active
 *
 *   Wenn dies falsch ist, so ist der Spieler nur ein Platzhalter im Spieler-
 *   Vektor, damit die Vektornummern der Spieler mit den Peer-Nummern von
 *   Network uebereinstimmen.
 *
 */

#pragma once

#include "../other/myalleg.h"

#include <list>

#include "../network/net_t.h" // net types: PlNr
#include "../lix/lix.h"
#include "../other/globals.h"

struct Tribe {

    struct Skill {
        LixEn::Ac  ac;
        int        nr;
        inline Skill() : ac(LixEn::NOTHING), nr(0) {}
    };

    struct Master {
        PlNr        number;
        std::string name;
        int         skill_sel;
        inline Master()
         : number(0), skill_sel(0) {}
    };

    typedef std::vector <Tribe>  ::iterator       It;
    typedef std::vector <Tribe>  ::const_iterator CIt;
    typedef std::list   <Master> ::iterator       MIt;
    typedef std::vector <Lixxie> ::iterator       LixIt;

    int         initial;
    int         required;

    int         lix_hatch;
    int         lix_saved;
    int         lix_saved_late; // after the goals have been locked
    int         lix_out; // change this only when killing/generating lixes.
    int         lix_exiting; // these have been scored, but keep game running
    int         spawnint_slow;
    int         spawnint_fast;
    int         spawnint;
    bool        nuke;

    unsigned long update_hatch;
    unsigned long update_saved; // last lix saved within timelimit
    int           hatch_next;

    std::vector <Skill>  skill;
    int                  skills_used;

    LixEn::Style         style;
    std::vector <Lixxie> lixvec;
    std::list   <Master> masters;

    Tribe();
    ~Tribe();

    void        return_skills(const LixEn::Ac, const int);
    std::string get_name() const;

    inline int get_still_playing()  const { return lix_out + lix_exiting
                                                 + lix_hatch;}
    inline int get_score()          const { return lix_saved; }
    inline int get_score_expected() const { return lix_saved
                                                 + lix_out + lix_hatch; }

};
