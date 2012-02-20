#include <string.h>
#include <sstream>
#include "track.h"
#include "user.h"
#include "number.h"
#include "path.h"
#include "art.h"
#include <taglib/taglib.h>
#include <taglib/mpegfile.h>
#include <taglib/vorbisfile.h>

using namespace std;

Track::Track(int id){
    _id = 0;
    _artistId = 0;
    if(id<=0) return;

    DB::Result r = DB::query(
        "SELECT tracks.title, tracks.user_id, users.name, tracks.visible, tracks.date FROM tracks CROSS JOIN users "
        "WHERE tracks.id = " + number(id) + " AND tracks.user_id = users.id");
    if(!r.empty()){
        _id = id;
        _title = r[0][0];
        _artistId = number(r[0][1]);
        _artist = r[0][2];
        _visible = r[0][3] == "t";
        _date = r[0][4];
    }
}

std::string Track::filePath(Format f) const{
    string format = f==Vorbis? "ogg" : "mp3";
    return eqbeatsDir() + "/tracks/" + number(_id) + "." + format;
}

void Track::setTitle(const std::string &nTitle){
    if(_title == nTitle) return;
    DB::query("UPDATE tracks SET title = $1 WHERE id = " + number(_id), nTitle);
    _title = nTitle;
    updateTags();
}

void Track::updateTags(Track::Format format){
    if(format == MP3){
        TagLib::MPEG::File mp3(filePath(Track::MP3).c_str());
        TagLib::Tag *t = mp3.tag();
        if(!t) return;
        t->setTitle(_title);
        t->setArtist(_artist);
        mp3.save(TagLib::MPEG::File::ID3v1 | TagLib::MPEG::File::ID3v2);
    } else if(format == Vorbis) {
        TagLib::Ogg::Vorbis::File vorbis(filePath(Track::Vorbis).c_str());
        TagLib::Tag *t = vorbis.tag();
        if(!t) return;
        t->setTitle(_title);
        t->setArtist(_artist);
        vorbis.save();
    }
}

std::string Track::url(int id){
    std::stringstream s;
    s << "/track/" << id;
    return s.str();
}

std::string Track::artUrl() const{
    string path = eqbeatsDir() + "/art/" + number(_id);
    if(access(path.c_str(), R_OK))
        return std::string();
    return (std::string) "/track/" + number(_id) + "/art";
}

void Track::setVisible(bool v){
    DB::query("UPDATE tracks SET visible = '" + (std::string)(v?"t":"f") + "' WHERE id = " + number(_id));
    _visible = v;
}

std::string Track::getNotes() const{
    DB::Result r = DB::query("SELECT notes FROM tracks WHERE id = " + number(_id));
    return r.empty() ? std::string() : r[0][0];
}
void Track::setNotes(const std::string &nNotes){
    DB::query("UPDATE tracks SET notes = $1 WHERE id = " + number(_id), nNotes);
}

std::string Track::url(Format f) const{
    string format = f==Vorbis? "vorbis" : "mp3";
    return (std::string) "/track/" + number(_id) + "/" + format;
}

Track Track::create(int nArtistId, const std::string &nTitle){
    Track t;
    DB::Result r = DB::query(
        "INSERT INTO tracks (user_id, title, date) VALUES "
        "("+number(nArtistId)+", $1, 'now') "
        "RETURNING id, date", nTitle);
    if(r.empty()) return t;
    return Track(number(r[0][0]), nTitle, nArtistId, User(nArtistId).name(), false, r[0][1]);
}

void Track::remove(){
    DB::query("DELETE FROM track_categories WHERE track_id = "+ number(_id));
    DB::query("DELETE FROM contest_submissions WHERE track_id = " + number(_id));
    DB::query("DELETE FROM votes WHERE track_id = " + number(_id));
    DB::query("DELETE FROM comments WHERE type = 'track' AND ref = " + number(_id));
    DB::query("DELETE FROM favorites WHERE type = 'track' AND ref = " + number(_id));
    DB::query("DELETE FROM tracks WHERE id = " + number(_id));
    string base = eqbeatsDir() + "/tracks/" + number(_id) + ".";
    string path = base + "mp3";
    unlink(path.c_str());
    path = base + "ogg";
    unlink(path.c_str());
    Art art(_id);
    if(art) art.remove();
    _id = 0;
}

int Track::getHits() const{
    DB::Result r = DB::query("SELECT hits FROM tracks WHERE id = " + number(_id));
    return number(r[0][0]);
}
void Track::hit(){
    DB::query("UPDATE tracks SET hits = hits+1 WHERE id = " + number(_id));
}

std::vector<Track> Track::resultToVector(const DB::Result &r){
    std::vector<Track> tracks(r.size());
    for(unsigned i=0; i<r.size(); i++)
        tracks[i] = Track(number(r[i][0]), r[i][1], number(r[i][2]), r[i][3], r[i][4]=="t", r[i][5]);
    return tracks;
}

#define SEL (std::string) "SELECT tracks.id, tracks.title, tracks.user_id, users.name, tracks.visible, tracks.date FROM tracks CROSS JOIN users " \
               "WHERE tracks.user_id = users.id AND "

std::vector<Track> Track::byArtist(int sArtistId, bool all){
    return resultToVector(DB::query(SEL + "tracks.user_id = " + number(sArtistId)
        + (all? "" : " AND tracks.visible = 't'") + " ORDER BY date DESC "));
}

std::vector<Track> Track::byCategory(int cat){
    return resultToVector(DB::query(
        "SELECT tracks.id, tracks.title, tracks.user_id, users.name, tracks.visible, tracks.date "
        "FROM tracks, users, track_categories "
        "WHERE tracks.user_id = users.id"
        " AND track_categories.cat_id = " + number(cat) +
        " AND track_categories.track_id = tracks.id"
        " AND visible='t' ORDER BY date DESC"));
}

std::vector<Track> Track::forContest(int cont){
    return resultToVector(DB::query(
        "SELECT tracks.id, tracks.title, tracks.user_id, users.name, tracks.visible, tracks.date FROM tracks, users, contest_submissions "
        "WHERE tracks.user_id = users.id"
        " AND tracks.id = contest_submissions.track_id"
        " AND contest_submissions.contest_id = " + number(cont) + " "
        "ORDER BY tracks.title ASC"));
}

std::vector<Track> Track::search(const std::string &q){
    if(q.empty()) return std::vector<Track>();
    std::vector<std::string> p;
    std::istringstream in(q);
    std::string buf;
    std::string sql;
    while(in){
        in >> buf;
        p.push_back("%"+buf+"%");
        sql += " AND (tracks.title ILIKE $" + number(p.size()) + " OR users.name ILIKE $" + number(p.size()) + ")";
    }
    return resultToVector(DB::query(SEL + "visible = 't'" + sql, p));
}

std::vector<Track> Track::exactSearch(const std::string &qartist, const std::string &qtitle){
    if(qtitle.empty()||qartist.empty()) return std::vector<Track>();
    return resultToVector(DB::query(SEL + "visible = 't' AND users.name = $1 AND tracks.title = $2", qartist, qtitle));
}

std::vector<Track> Track::latest(int n){
    return resultToVector(DB::query(SEL + "visible = 't' ORDER BY date DESC LIMIT " + number(n)));
}

std::vector<Track> Track::random(int n){
    return resultToVector(DB::query(SEL + "visible = 't' ORDER BY random() LIMIT " + number(n)));
}

std::vector<Track> Track::popular(int n){
    return resultToVector(DB::query(SEL + "visible = 't' ORDER BY hits DESC LIMIT " + number(n)));
}

std::vector<Track> Track::favorites(int uid){
    return resultToVector(DB::query(
        "SELECT tracks.id, tracks.title, tracks.user_id, users.name, tracks.visible, tracks.date FROM tracks, users, favorites "
        "WHERE tracks.user_id = users.id "
        "AND favorites.type = 'track' "
        "AND favorites.user_id = " + number(uid) + " "
        "AND favorites.ref = tracks.id "
        "AND tracks.visible = 't' "
        "ORDER BY users.name ASC"));
}

int Track::favoritesCount() const{
    DB::Result r = DB::query("SELECT COUNT(*) FROM favorites "
        "WHERE type = 'track' AND ref = " + number(_id));
    return r.empty() ? -1 : number(r[0][0]);
}

void Track::bump(){
    if(_id<=0) return;
    DB::query("UPDATE tracks SET date = 'now' WHERE id = " + number(_id));
}
