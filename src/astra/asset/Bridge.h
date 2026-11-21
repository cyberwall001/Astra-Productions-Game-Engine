// asset/Bridge.h - the Blender bridge inbox.
//
// Blender cannot host a live C++ plugin, so the bridge is a folder contract instead: the
// companion addon (tools/blender/astra_bridge.py) exports the current selection as OBJ and drops
// it into <engine data>/bridge/inbox with a small JSON sidecar describing the export. The editor
// scans that folder on demand (and on startup) and imports whatever it finds into the level's
// mesh library, spawning an actor per file.
//
// Because it is just files on disk it is fully testable headlessly, works over a network share,
// and never leaves a half-written mesh in the level if Blender crashes mid-export.
#pragma once

#include <string>
#include <vector>

#include "astra/asset/MeshBuilder.h"
#include "astra/scene/Scene.h"

namespace astra::asset {

struct BridgeJob {
    std::string objPath;
    std::string metaPath;      // may be empty: the OBJ alone is enough
    std::string name = "Blender mesh";
    std::string material;      // material hint from the sidecar, e.g. "Hologram"
    bool placeholder = false;  // the addon is connected but has not exported yet
};

struct BridgeImportResult {
    int imported = 0;
    int skipped = 0;
    int triangles = 0;
    std::vector<std::string> actorNames;
    std::vector<std::string> errors;
};

// <engine data>/bridge/inbox - created on demand.
std::string InboxDirectory();
// <engine data>/bridge/status.json - the addon writes heartbeats here so the UI can show whether
// Blender is connected.
std::string StatusPath();

// Lists jobs in the inbox, newest name order. Unreadable sidecars are tolerated.
std::vector<BridgeJob> ScanInbox();

// Imports one job into `scene`: the mesh joins the level's mesh library and one actor is spawned
// at `spawn` (a custom-mesh actor with the imported geometry attached).
bool ImportJob(scene::Scene& scene, const BridgeJob& job, const scene::Vec3& spawn,
               BridgeImportResult& result);

// Imports everything in the inbox. Files are moved to bridge/imported so a later scan does not
// re-import them.
BridgeImportResult ImportInbox(scene::Scene& scene, bool moveImported = true);

// Human-readable state for the UI: "Connected (Blender 4.2)", "Not connected", ...
std::string BlenderStatus();

// Writes a tiny OBJ + sidecar into the inbox. Used by tests and by "Send test mesh".
bool WriteBridgeJob(const std::string& name, const scene::MeshAsset& mesh, const std::string& material,
                    std::string* error = nullptr);

}  // namespace astra::asset
