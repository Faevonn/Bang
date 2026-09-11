#include "TestSupport.hpp"
#include "bang/LibraryCatalog.hpp"
#include "bang/LibraryStore.hpp"

#include <iostream>
#include <utility>
#include <vector>

using bang::test::require;

namespace {

std::vector<std::int64_t> trackIds(
    const bang::LibraryCatalog& catalog, std::int64_t playlist)
{
    std::vector<std::int64_t> ids;
    for (const auto& listing : catalog.playlistTracks(playlist)) {
        ids.push_back(listing.track.id);
    }
    return ids;
}

void checkPlaylistOrder()
{
    bang::test::TemporaryDirectory directory;
    std::int64_t playlist = 0;
    std::vector<std::int64_t> ids;
    {
        bang::LibraryStore store(directory.path);
        bang::LibraryCatalog catalog(store);
        playlist = store.createPlaylist("Order test");
        const auto otherPlaylist = store.createPlaylist("Unchanged playlist");
        for (const char* name : { "A", "B", "C", "D" }) {
            const auto track = store.addTrack({ .contentHash = name, .title = name });
            ids.push_back(track.id);
            store.addToPlaylist(playlist, track.id);
            store.addToPlaylist(otherPlaylist, track.id);
        }
        require(trackIds(catalog, playlist) == ids, "preserve insertion order");

        store.addToPlaylist(playlist, ids[1]);
        require(trackIds(catalog, playlist) == ids, "ignore duplicate membership");

        store.moveWithinPlaylist(playlist, 0, 2);
        require(trackIds(catalog, playlist)
                == std::vector<std::int64_t> { ids[1], ids[2], ids[0], ids[3] },
            "move a track toward the end");
        store.moveWithinPlaylist(playlist, 3, 0);
        const std::vector<std::int64_t> moved { ids[3], ids[1], ids[2], ids[0] };
        require(trackIds(catalog, playlist) == moved, "move a track toward the start");
        require(trackIds(catalog, otherPlaylist) == ids, "leave other playlists unchanged");

        store.moveWithinPlaylist(playlist, 1, 1);
        require(trackIds(catalog, playlist) == moved, "moving to the same position is a no-op");
        for (const auto [from, to] : { std::pair<std::size_t, std::size_t> { 9, 0 },
                 { 0, 9 } }) {
            bool rejected = false;
            try {
                store.moveWithinPlaylist(playlist, from, to);
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            require(rejected, "reject an out-of-range playlist move");
            require(trackIds(catalog, playlist) == moved, "roll back a rejected move");
        }

        store.removeFromPlaylist(playlist, ids[1]);
        store.removeFromPlaylist(playlist, ids[1]);
        require(trackIds(catalog, playlist)
                == std::vector<std::int64_t> { ids[3], ids[2], ids[0] },
            "remove membership without disturbing relative order");
        require(store.trackById(ids[1]).has_value(), "playlist removal preserves the library track");
        store.addToPlaylist(playlist, ids[1]);
        require(trackIds(catalog, playlist)
                == std::vector<std::int64_t> { ids[3], ids[2], ids[0], ids[1] },
            "append after removal and transaction rollback");
    }
    bang::LibraryStore reopened(directory.path);
    require(trackIds(bang::LibraryCatalog(reopened), playlist)
            == std::vector<std::int64_t> { ids[3], ids[2], ids[0], ids[1] },
        "persist playlist order across reopening the database");
}

} // namespace

int main()
{
    try {
        checkPlaylistOrder();
        std::cout << "PASS playlist ordering and persistence\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
