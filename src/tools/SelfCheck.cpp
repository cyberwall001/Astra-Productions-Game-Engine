// tools/SelfCheck.cpp - dependency-free test suite for the engine's non-UI layers.
//
//   astra_selfcheck            run every test
//   astra_selfcheck --list     list test names
//   astra_selfcheck --filter X run tests whose name contains X
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "astra/app/Settings.h"
#include "astra/auth/Account.h"
#include "astra/auth/AuthService.h"
#include "astra/core/FileIO.h"
#include "astra/core/Paths.h"
#include "astra/core/Strings.h"
#include "astra/core/Time.h"
#include "astra/crypto/Encoding.h"
#include "astra/crypto/Random.h"
#include "astra/crypto/Sha256.h"
#include "astra/data/Json.h"
#include "astra/github/GitHubJson.h"
#include "astra/licensing/AccessCode.h"
#include "astra/licensing/CodeRegistry.h"
#include "astra/licensing/MasterKey.h"
#include "astra/project/Project.h"
#include "astra/scene/Reflection.h"
#include "astra/image/Image.h"
#include "astra/render/Camera.h"
#include "astra/game/Player.h"
#include "astra/asset/Bridge.h"
#include "astra/asset/Material.h"
#include "astra/asset/MeshBuilder.h"
#include "astra/render/Pathtracer.h"
#include "astra/script/Host.h"
#include "astra/render/ViewportRenderer.h"
#include "astra/scene/Reflection.h"
#include "astra/scene/Scene.h"

namespace {

int gChecks = 0;
int gFailures = 0;
std::string gCurrentTest;

void Check(bool condition, const char* expression, const char* file, int line) {
    ++gChecks;
    if (condition) return;
    ++gFailures;
    std::fprintf(stderr, "  FAIL [%s] %s (%s:%d)\n", gCurrentTest.c_str(), expression, file, line);
}

#define CHECK(expr) Check((expr), #expr, __FILE__, __LINE__)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        const auto& _a = (a);                                                 \
        const auto& _b = (b);                                                 \
        ++gChecks;                                                            \
        if (!(_a == _b)) {                                                    \
            ++gFailures;                                                      \
            std::fprintf(stderr, "  FAIL [%s] %s == %s (%s:%d)\n",            \
                         gCurrentTest.c_str(), #a, #b, __FILE__, __LINE__);   \
        }                                                                     \
    } while (false)

std::string ToHex(const std::uint8_t* data, std::size_t size) {
    return astra::crypto::HexEncode(data, size);
}

// ---------------------------------------------------------------------------

void TestSha256Vectors() {
    CHECK_EQ(astra::crypto::Sha256::HexDigest(""),
             std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(astra::crypto::Sha256::HexDigest("abc"),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(astra::crypto::Sha256::HexDigest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));

    // Multi-update streaming must match a single-shot hash.
    astra::crypto::Sha256 hasher;
    hasher.Update(std::string_view("a"));
    hasher.Update(std::string_view("bc"));
    std::uint8_t digest[32];
    hasher.Final(digest);
    CHECK_EQ(ToHex(digest, sizeof(digest)),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));

    // Long input exercises the length padding path.
    std::string longText(1000, 'a');
    CHECK_EQ(astra::crypto::Sha256::HexDigest(longText),
             std::string("41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3"));
}

void TestHmac() {
    // RFC 4231 test case 2.
    const std::string key = "Jefe";
    const std::string data = "what do ya want for nothing?";
    const auto mac = astra::crypto::HmacSha256(astra::ByteView(key), astra::ByteView(data));
    CHECK_EQ(ToHex(mac.data(), mac.size()),
             std::string("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));

    // Constant time compare behaves.
    const std::uint8_t a[4] = {1, 2, 3, 4};
    const std::uint8_t b[4] = {1, 2, 3, 4};
    const std::uint8_t c[4] = {1, 2, 3, 5};
    CHECK(astra::crypto::ConstantTimeEquals(astra::ByteView(a, 4), astra::ByteView(b, 4)));
    CHECK(!astra::crypto::ConstantTimeEquals(astra::ByteView(a, 4), astra::ByteView(c, 4)));
}

void TestPbkdf2() {
    // PBKDF2-HMAC-SHA256("password", "salt", 1, 32).
    const auto derived = astra::crypto::Pbkdf2Sha256("password", astra::ByteView(std::string_view("salt")), 1);
    CHECK_EQ(ToHex(derived.data(), derived.size()),
             std::string("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"));

    // Two different iterations must not collide.
    const auto more = astra::crypto::Pbkdf2Sha256("password", astra::ByteView(std::string_view("salt")), 2);
    CHECK(ToHex(derived.data(), derived.size()) != ToHex(more.data(), more.size()));
}

void TestBase32() {
    const astra::Bytes raw = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC};
    const std::string encoded = astra::crypto::Base32Encode(raw);
    CHECK_EQ(encoded.size(), static_cast<std::size_t>(16));
    astra::Bytes decoded;
    CHECK(astra::crypto::Base32Decode(encoded, decoded));
    CHECK_EQ(decoded.size(), raw.size());
    CHECK(decoded == raw);

    // Forgiving decode: lower case, dashes, ambiguous characters.
    astra::Bytes again;
    const std::string messy = "abcd-efgh-jkmn-pqrs";
    CHECK(astra::crypto::Base32Decode(messy, again));
    CHECK(!again.empty());

    CHECK_EQ(astra::crypto::GroupWithDashes("ABCDEFGH", 4), std::string("ABCD-EFGH"));
    CHECK_EQ(astra::crypto::NormalizeCode("k7qm-3xpd 9rta.b2vh"), std::string("K7QM3XPD9RTAB2VH"));
}

void TestJson() {
    const std::string text = R"({
        "name": "Astra",
        "version": 2,
        "active": true,
        "nested": {"list": [1, 2, 3], "empty": [], "obj": {}},
        "escaped": "line\nbreak \"quoted\"",
        "unicode": "\u0041\u00e9",
        "nothing": null
    })";

    astra::json::Value root;
    std::string error;
    CHECK(astra::json::Value::Parse(text, root, &error));
    if (!error.empty()) std::printf("  parse error: %s\n", error.c_str());

    CHECK_EQ(root["name"].AsString(), std::string("Astra"));
    CHECK_EQ(root["version"].AsInt(), 2LL);
    CHECK(root["active"].AsBool());
    CHECK_EQ(root["nested"]["list"].Size(), static_cast<std::size_t>(3));
    CHECK_EQ(root["nested"]["list"].At(2).AsInt(), 3LL);
    CHECK(root["nested"]["empty"].IsArray());
    CHECK(root["nothing"].IsNull());
    CHECK_EQ(root["escaped"].AsString(), std::string("line\nbreak \"quoted\""));
    CHECK_EQ(root["unicode"].AsString(), std::string("A\xC3\xA9"));

    // Round trip.
    const std::string dumped = root.Dump(true);
    astra::json::Value reparsed;
    CHECK(astra::json::Value::Parse(dumped, reparsed));
    CHECK_EQ(reparsed["nested"]["list"].At(1).AsInt(), 2LL);

    // Malformed inputs must fail cleanly.
    astra::json::Value ignored;
    CHECK(!astra::json::Value::Parse("{\"a\": }", ignored));
    CHECK(!astra::json::Value::Parse("[1, 2", ignored));
    CHECK(!astra::json::Value::Parse("{} trailing", ignored));

    // Compact output is valid JSON too.
    CHECK(astra::json::Value::Parse(root.Dump(false), reparsed));
}

void TestAccessCodes() {
    const astra::Bytes secret = astra::license::MasterKeyBytes();
    CHECK_EQ(secret.size(), static_cast<std::size_t>(32));

    astra::license::CodePayload payload;
    payload.role = astra::license::Role::Staff;
    payload.durationDays = 30;
    payload.issuedDay = astra::license::DayFromUtc(astra::time::NowUtc());
    payload.nonce = 0x1234;

    const astra::license::AccessCode code = astra::license::IssueCode(payload, secret);
    CHECK_EQ(code.text.size(), static_cast<std::size_t>(19));  // 16 chars + 3 dashes
    CHECK(astra::license::IsCompleteCode(code.text));

    const astra::license::CodeResult ok =
        astra::license::VerifyCode(code.text, secret, astra::time::NowUtc());
    CHECK(ok.ok());
    CHECK_EQ(static_cast<int>(ok.code.payload.role), static_cast<int>(astra::license::Role::Staff));
    CHECK_EQ(ok.code.payload.durationDays, 30);
    CHECK(!ok.code.neverExpires);

    // Tampering must fail the HMAC check.
    std::string tampered = code.text;
    tampered[0] = tampered[0] == 'A' ? 'B' : 'A';
    const astra::license::CodeResult bad =
        astra::license::VerifyCode(tampered, secret, astra::time::NowUtc());
    CHECK(!bad.ok());
    CHECK(bad.error == astra::license::CodeError::BadSignature);

    // Wrong secret must fail.
    astra::Bytes wrongSecret = secret;
    wrongSecret[0] ^= 0xFF;
    CHECK(!astra::license::VerifyCode(code.text, wrongSecret, astra::time::NowUtc()).ok());

    // Expired code (issued 400 days ago, 30 day window).
    astra::license::CodePayload old = payload;
    old.issuedDay = astra::license::DayFromUtc(astra::time::NowUtc()) - 400;
    const astra::license::AccessCode oldCode = astra::license::IssueCode(old, secret);
    const astra::license::CodeResult expired =
        astra::license::VerifyCode(oldCode.text, secret, astra::time::NowUtc());
    CHECK(!expired.ok());
    CHECK(expired.error == astra::license::CodeError::Expired);

    // Future-dated code is not valid yet.
    astra::license::CodePayload future = payload;
    future.issuedDay = astra::license::DayFromUtc(astra::time::NowUtc()) + 30;
    CHECK(!astra::license::VerifyCode(astra::license::IssueCode(future, secret).text, secret,
                                      astra::time::NowUtc())
               .ok());

    // Never-expiring code.
    astra::license::CodePayload forever = payload;
    forever.durationDays = 0;
    const astra::license::CodeResult never =
        astra::license::VerifyCode(astra::license::IssueCode(forever, secret).text, secret,
                                   astra::time::NowUtc());
    CHECK(never.ok());
    CHECK(never.code.neverExpires);

    // Malformed inputs.
    CHECK(astra::license::VerifyCode("", secret, 0).error == astra::license::CodeError::Empty);
    CHECK(astra::license::VerifyCode("ABCD", secret, 0).error ==
          astra::license::CodeError::Malformed);

    // Masking hides the payload.
    const std::string masked = astra::license::MaskCode(astra::crypto::NormalizeCode(code.text));
    CHECK_EQ(masked.size(), static_cast<std::size_t>(19));
    CHECK_EQ(masked.substr(9, 5), std::string("-****"));

    // Single-use registry.
    astra::license::CodeRegistry registry(astra::paths::Join(astra::paths::EnsureDataDir("tests"),
                                                             "used_codes.json"));
    const std::string normalized = astra::crypto::NormalizeCode(code.text);
    CHECK(!registry.IsUsed(normalized));
    CHECK(registry.MarkUsed(normalized, "acc_test", astra::time::NowUtc()));
    CHECK(registry.IsUsed(normalized));
    CHECK(!registry.MarkUsed(normalized, "acc_test", astra::time::NowUtc()));
    CHECK(registry.Load());
    CHECK(registry.IsUsed(normalized));
}

void TestAccounts() {
    using namespace astra::auth;
    CHECK(ValidateUsername("ada").ok);
    CHECK(ValidateUsername("ada.reyes").ok);
    CHECK(!ValidateUsername("a").ok);
    CHECK(!ValidateUsername("ada reyes").ok);
    CHECK(ValidateDisplayName("Ada Reyes").ok);
    CHECK(!ValidateDisplayName("A").ok);
    CHECK(ValidateEmail("ada@astrapictures.com").ok);
    CHECK(ValidateEmail("").ok);
    CHECK(!ValidateEmail("ada@nope").ok);

    CHECK(ValidatePassword("StormChase#2026", astra::license::Role::User).ok);
    CHECK(ValidatePassword("StormChase#2026", astra::license::Role::Staff).ok);
    CHECK(!ValidatePassword("short1", astra::license::Role::User).ok);
    CHECK(!ValidatePassword("allletterslong", astra::license::Role::User).ok);
    CHECK(ValidatePassword("StaffPass123", astra::license::Role::Staff).ok);  // exactly 12
    CHECK(!ValidatePassword("StaffPass12", astra::license::Role::Staff).ok);   // 11 chars
    CHECK(ValidatePassword("StaffPass1234", astra::license::Role::Staff).ok);

    CHECK_EQ(EvaluatePassword("abc", astra::license::Role::User).score, 0);
    CHECK(EvaluatePassword("Str0ng!Password", astra::license::Role::User).score >= 3);

    AccountStore store(astra::paths::Join(astra::paths::EnsureDataDir("tests"), "accounts.json"));
    store.Load();
    for (const auto& existing : store.Accounts()) store.Remove(existing.id);

    Account account = AccountStore::CreateAccount("ada.reyes", "Ada Reyes", "Mesocyclone#77",
                                                  astra::license::Role::Staff, "ada@astra.dev",
                                                  "K7QM-****-****-B2VH");
    CHECK_EQ(account.username, std::string("ada.reyes"));
    CHECK(account.id.rfind("acc_", 0) == 0);
    std::string error;
    CHECK(store.Add(std::move(account), &error));
    CHECK(error.empty());

    const Account* stored = store.FindByUsername("ADA.REYES");
    CHECK(stored != nullptr);
    if (stored) {
        CHECK(VerifyPasswordHash("Mesocyclone#77", stored->saltHex, stored->hashHex,
                                 stored->iterations));
        CHECK(!VerifyPasswordHash("wrong", stored->saltHex, stored->hashHex, stored->iterations));
        CHECK_EQ(stored->role, astra::license::Role::Staff);
        CHECK_EQ(stored->Initials(), std::string("AR"));
    }

    // Duplicate usernames are rejected case-insensitively.
    Account duplicate = AccountStore::CreateAccount("ada.reyes", "Other", "Mesocyclone#99",
                                                    astra::license::Role::User);
    CHECK(!store.Add(std::move(duplicate), &error));

    // Persistence round trip.
    AccountStore reloaded(store.Path());
    CHECK(reloaded.Load());
    CHECK_EQ(reloaded.Accounts().size(), static_cast<std::size_t>(1));
}

void TestRegistrationFlow() {
    astra::auth::AuthService auth;
    auth.Initialize();

    const astra::Bytes secret = astra::license::MasterKeyBytes();
    astra::license::CodePayload payload;
    payload.role = astra::license::Role::User;
    payload.durationDays = 7;
    payload.issuedDay = astra::license::DayFromUtc(astra::time::NowUtc());
    payload.nonce = 0x0BAD;
    const astra::license::AccessCode code = astra::license::IssueCode(payload, secret);

    // Registration validates the form before touching the registry.
    auto result = auth.Register(code.text, "x", "X", "Password123", "Password123", "", true);
    CHECK(!result.ok);

    result = auth.Register(code.text, "chase.lead", "Chase Lead", "Password123", "Mismatch123",
                           "", true);
    CHECK(!result.ok);

    result = auth.Register(code.text, "chase.lead", "Chase Lead", "Password123", "Password123", "",
                           false);
    CHECK(!result.ok);  // terms not accepted

    result = auth.Register(code.text, "chase.lead", "Chase Lead", "Password123", "Password123", "",
                           true);
    CHECK(result.ok);
    CHECK(!result.accountId.empty());
    CHECK(auth.IsSignedIn());

    const astra::auth::Account* account = auth.Current();
    CHECK(account != nullptr);
    if (account) {
        CHECK_EQ(account->username, std::string("chase.lead"));
        CHECK_EQ(account->accessCodeMasked, astra::license::MaskCode(
                                                astra::crypto::NormalizeCode(code.text)));
    }

    // The same code cannot be redeemed twice.
    astra::auth::AuthService second;
    second.Initialize();
    const auto reuse = second.Register(code.text, "other.user", "Other", "Password123",
                                       "Password123", "", true);
    CHECK(!reuse.ok);

    // Sign in works, wrong passwords do not.
    auth.SignOut();
    CHECK(!auth.IsSignedIn());
    CHECK(!auth.SignIn("chase.lead", "wrong", false).ok);
    CHECK(auth.SignIn("chase.lead", "Password123", true).ok);
    CHECK(auth.IsSignedIn());

    // Remember-me session restores.
    astra::auth::AuthService restored;
    restored.Initialize();
    CHECK(restored.RestoreSession().ok);
}

void TestScene() {
    astra::scene::Scene world = astra::scene::Scene::CreateStormScene("Test Storm");
    CHECK(world.Count() >= 6);
    CHECK(!world.Roots().empty());
    CHECK(world.ActiveCount() > 0);

    astra::scene::Entity* storm = world.FindByName("Supercell 03 / Mesocyclone");
    CHECK(storm != nullptr);
    if (storm) {
        CHECK(storm->Has(astra::scene::Component::Weather));
        CHECK(storm->weather.TornadoProbability() > 0.3f);
        CHECK(storm->weather.InflowLayerShear() > 0.0f);
    }

    const astra::scene::EntityId rootId = world.Roots().front();
    std::size_t childCountBefore = 0;
    if (const astra::scene::Entity* root = world.Find(rootId)) childCountBefore = root->children.size();
    const astra::scene::EntityId childId = world.CreateEntity("Self Check Child", rootId);
    CHECK(world.Find(childId) != nullptr);
    if (const astra::scene::Entity* root = world.Find(rootId)) {
        CHECK_EQ(root->children.size(), childCountBefore + 1);
    }

    // Cycles and self-parenting are rejected.
    CHECK(!world.Reparent(rootId, childId));
    CHECK(!world.Reparent(rootId, rootId));
    CHECK(world.Reparent(childId, astra::scene::kInvalidEntity));

    // Serialization round trip.
    const std::string json = world.ToJson();
    astra::scene::Scene reloaded;
    std::string error;
    CHECK(reloaded.FromJson(json, &error));
    if (!error.empty()) std::printf("  scene error: %s\n", error.c_str());
    CHECK_EQ(reloaded.Count(), world.Count());
    CHECK_EQ(reloaded.Name(), world.Name());
    const astra::scene::Entity* reloadedStorm = reloaded.FindByName("Supercell 03 / Mesocyclone");
    CHECK(reloadedStorm != nullptr);
    if (reloadedStorm) {
        CHECK(reloadedStorm->Has(astra::scene::Component::Weather));
        CHECK_EQ(static_cast<int>(reloadedStorm->weather.preset),
                 static_cast<int>(astra::scene::StormPreset::Supercell));
        // Children must point back at their parent after the round trip.
        for (astra::scene::EntityId childEntity : reloadedStorm->children) {
            const astra::scene::Entity* child = reloaded.Find(childEntity);
            CHECK(child != nullptr);
            if (child) CHECK_EQ(child->parent, reloadedStorm->id);
        }
    }

    // Destroying a parent removes descendants.
    const std::size_t before = reloaded.Count();
    CHECK(reloaded.DestroyEntity(rootId));
    CHECK(reloaded.Count() < before);

    // Reflection model covers the weather component.
    astra::scene::Scene demo = astra::scene::Scene::CreateStormScene();
    if (astra::scene::Entity* entity = demo.FindByName("Vortex Core")) {
        const auto groups = astra::scene::BuildInspectorModel(*entity);
        CHECK(groups.size() >= 4);
        bool sawWeather = false;
        for (const auto& group : groups) {
            if (group.title == "Weather Field") sawWeather = true;
        }
        CHECK(sawWeather);
    }
}

void TestSampleLevel() {
    astra::scene::Scene level = astra::scene::Scene::CreateSampleLevel("Level 01 - Sandbox");
    CHECK(level.Name() == "Level 01 - Sandbox");
    CHECK(level.Count() >= 10);

    // The outliner groups mirror the Unreal default level.
    for (const char* rootName : {"Lighting", "Geometry", "Gameplay", "Cameras"}) {
        CHECK(level.FindByName(rootName) != nullptr);
    }

    astra::scene::Entity* cube = level.FindByName("Cube");
    CHECK(cube != nullptr);
    if (cube) {
        CHECK(cube->Has(astra::scene::Component::Mesh));
        CHECK_EQ(static_cast<int>(cube->mesh.shape),
                 static_cast<int>(astra::scene::MeshShape::Cube));
        CHECK(cube->mesh.visible);
    }
    CHECK(level.FindByName("Weather Volume") != nullptr);

    // Meshes round trip through JSON with shape, size and colour intact.
    astra::scene::Scene reloaded;
    std::string error;
    CHECK(reloaded.FromJson(level.ToJson(), &error));
    if (!error.empty()) std::printf("  sample level error: %s\n", error.c_str());
    const astra::scene::Entity* cubeCopy = reloaded.FindByName("Cube");
    CHECK(cubeCopy != nullptr);
    if (cube && cubeCopy) {
        CHECK_EQ(static_cast<int>(cubeCopy->mesh.shape),
                 static_cast<int>(cube->mesh.shape));
        CHECK(cubeCopy->mesh.size.x == cube->mesh.size.x);
        CHECK(cubeCopy->mesh.baseColor.y == cube->mesh.baseColor.y);
    }

    // The Details panel exposes the static mesh group for a mesh actor.
    if (cube) {
        bool sawMesh = false;
        for (const auto& group : astra::scene::BuildInspectorModel(*cube)) {
            if (group.title == "Static Mesh") sawMesh = true;
        }
        CHECK(sawMesh);
    }

    // Every shape has a display name for the enum picker.
    int shapeCount = 0;
    const char* const* shapeNames = astra::scene::MeshShapeNames(shapeCount);
    CHECK(shapeCount >= 5);
    CHECK(shapeNames != nullptr);
    if (shapeNames) CHECK(shapeNames[0] != nullptr);

    // The storm sample is still loadable - it is a template now, not the default.
    astra::scene::Scene storm = astra::scene::Scene::CreateStormScene("Template - Storm");
    CHECK(storm.Count() > 6);
    CHECK(storm.FindByName("Supercell 03 / Mesocyclone") != nullptr);
}

void TestPlayer() {
    astra::scene::Scene level = astra::scene::Scene::CreateSampleLevel("PIE");
    astra::game::Player player;
    CHECK(astra::game::SpawnFromLevel(player, level));
    // The sample level's Player Start sits above the floor, facing into it.
    CHECK(player.position.z > 0.0f);
    CHECK(player.Eye().y >= player.eyeHeight);  // the eye sits at eye height above the floor

    const astra::scene::Vec3 start = player.position;
    for (int i = 0; i < 60; ++i) {
        astra::game::PlayerInput input;
        input.forward = true;
        player.Update(1.0f / 60.0f, input);
    }
    const float walked = std::sqrt((player.position.x - start.x) * (player.position.x - start.x) +
                                   (player.position.z - start.z) * (player.position.z - start.z));
    CHECK(walked > 3.0f);
    CHECK(walked < 6.0f);  // walk speed is 4.6 m/s
    CHECK(player.position.y >= 0.0f);

    // Jump, then let gravity bring the pawn back down.
    astra::game::PlayerInput jump;
    jump.jump = true;
    player.Update(1.0f / 60.0f, jump);
    CHECK(player.velocityY > 0.0f);
    CHECK(!player.onGround);
    for (int i = 0; i < 120; ++i) player.Update(1.0f / 60.0f, astra::game::PlayerInput{});
    CHECK(player.onGround);

    // Strafing moves along the right vector, never along the view direction.
    const astra::scene::Vec3 before = player.position;
    astra::game::PlayerInput strafe;
    strafe.right = true;
    for (int i = 0; i < 30; ++i) player.Update(1.0f / 60.0f, strafe);
    const astra::scene::Vec3 right = player.Right();
    const astra::scene::Vec3 delta(player.position.x - before.x, 0.0f, player.position.z - before.z);
    CHECK(delta.x * right.x + delta.z * right.z > 0.5f);

    // Look input turns the pawn.
    const float yawBefore = player.yawDeg;
    astra::game::PlayerInput look;
    look.lookDeltaX = 40.0f;
    player.Update(1.0f / 60.0f, look);
    CHECK(player.yawDeg != yawBefore);

    // The camera maps to the eye and looks along Forward().
    const astra::render::Camera camera = player.ViewCamera();
    CHECK(std::abs(camera.target.y - player.Eye().y) < 1.0f);
    CHECK(camera.distance <= 1.0f);

    // Levels without a spawn point still produce a usable player.
    astra::scene::Scene empty = astra::scene::Scene::CreateEmptyScene("empty");
    astra::game::Player lost;
    astra::game::SpawnFromLevel(lost, empty);
    CHECK(lost.eyeHeight > 0.0f);
}

void TestPathtracer() {
    astra::scene::Scene level = astra::scene::Scene::CreateSampleLevel("trace");
    astra::render::TraceScene baked;
    baked.Build(level);
    CHECK(baked.TriangleCount() > 100);
    CHECK(baked.TriangleCount() < 20000);

    astra::render::TraceSettings settings;
    settings.width = 64;
    settings.height = 36;
    settings.samples = 4;
    settings.maxBounces = 2;

    astra::render::TraceStats stats;
    astra::render::Camera camera;
    camera.target = astra::scene::Vec3(0.0f, 1.0f, 0.0f);
    camera.distance = 14.0f;
    camera.yawDeg = 20.0f;
    camera.pitchDeg = 18.0f;

    const astra::image::Buffer image = astra::render::Render(baked, camera, settings, &stats);
    CHECK(!image.Empty());
    CHECK_EQ(image.width, 64);
    CHECK_EQ(image.height, 36);
    CHECK(stats.triangles > 100);
    CHECK(stats.rays > 0);
    CHECK(stats.threads >= 1);

    // Nothing may be fully black or fully blown out: the tonemap should keep detail.
    double luminance = 0.0;
    int bright = 0;
    int dark = 0;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const astra::image::Color c = image.GetPixel(x, y);
            const double l = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
            luminance += l;
            if (l > 0.995) ++bright;
            if (l < 0.02) ++dark;
        }
    }
    const double pixels = static_cast<double>(image.width) * image.height;
    luminance /= pixels;
    CHECK(luminance > 0.08);
    CHECK(luminance < 0.92);
    CHECK(bright < image.width * image.height * 9 / 10);
    CHECK(dark < image.width * image.height * 9 / 10);

    // Ridiculously small requests are clamped instead of crashing, and the backend table is honest.
    settings.width = 4;
    settings.height = 3;
    settings.samples = 1;
    const astra::image::Buffer tiny = astra::render::Render(baked, camera, settings, nullptr);
    CHECK(tiny.width >= 4);
    CHECK(tiny.height >= 3);
    CHECK(std::string(astra::render::TraceBackendName(astra::render::TraceBackend::Cpu)) != "");
    CHECK(astra::render::TraceBackendAvailable(astra::render::TraceBackend::Cpu));
    CHECK(!astra::render::TraceBackendAvailable(astra::render::TraceBackend::Gpu));

    // PNG round-trip of an offline render.
    const std::string path = astra::paths::Join(astra::paths::EngineDataDir(),
                                                "selfcheck-trace.png");
    CHECK(astra::render::RenderToFile(level, camera, settings, path, nullptr));
    astra::image::Buffer reloaded;
    CHECK(astra::image::ReadPng(path, reloaded));
    CHECK_EQ(reloaded.width, tiny.width);
    std::remove(path.c_str());
}

void TestPresets() {
    int count = 0;
    const char* const* shaders = astra::scene::MaterialShaderNames(count);
    CHECK_EQ(count, astra::scene::kMaterialShaderCount);
    for (int i = 0; i < count; ++i) {
        CHECK(shaders[i] != nullptr);
        CHECK(std::string(astra::scene::MaterialShaderName(static_cast<astra::scene::MaterialShader>(i))) ==
              std::string(shaders[i]));
    }

    int presets = 0;
    const char* const* names = astra::scene::ParticlePresetNames(presets);
    CHECK(presets >= 5);
    for (int i = 0; i < presets; ++i) {
        CHECK(names[i] != nullptr);
        astra::scene::ParticleComponent particles;
        particles.preset = static_cast<astra::scene::ParticlePreset>(i);
        particles.rate = 0.0f;
        particles.lifetime = 0.0f;
        particles.speed = 0.0f;
        astra::scene::ApplyParticlePreset(particles);
        CHECK(particles.rate > 0.0f);
        CHECK(particles.lifetime > 0.0f);
        CHECK(particles.speed >= 0.0f);
        CHECK(particles.startSize > 0.0f);
    }

    // Scripts can drive the same presets and the Blender inbox.
    {
        astra::scene::Scene scripted = astra::scene::Scene::CreateSampleLevel("script presets");
        astra::script::Host host(scripted);
        const astra::script::ScriptRun run = host.Run(
            "let names = particlePresets()\n"
            "print(\"presets:\", len(names))\n"
            "let a = spawnActor(\"FX\", \"cube\", 3.0, 1.0, 0.0)\n"
            "setParticles(a, \"Fire\", 40.0)\n"
            "setMaterial(a, \"Hologram\", 0.2, 0.9, 0.0)\n",
            "presets.astra");
        CHECK(run.result.ok);
        if (!run.result.ok) std::fprintf(stderr, "  script: %s\n", run.result.error.c_str());
        const astra::scene::Entity* fx = scripted.FindByName("FX");
        CHECK(fx != nullptr);
        if (fx) {
            CHECK(fx->Has(astra::scene::Component::Particles));
            CHECK(fx->particles.preset == astra::scene::ParticlePreset::Fire);
            CHECK(std::abs(fx->particles.rate - 40.0f) < 0.01f);
            CHECK(fx->material.shader == astra::scene::MaterialShader::Hologram);
        }
    }

    // The mesh library survives a JSON round-trip with materials and particles attached.
    astra::scene::Scene installed;
    installed.SetName("presets");
    const astra::scene::EntityId id = installed.CreateEntity("Pillar");
    installed.Find(id)->Add(astra::scene::Component::Mesh | astra::scene::Component::Material |
                            astra::scene::Component::Particles);
    installed.Find(id)->material.shader = astra::scene::MaterialShader::Hologram;
    installed.Find(id)->material.opacity = 0.6f;
    installed.Find(id)->particles.preset = astra::scene::ParticlePreset::Fire;
    astra::scene::ApplyParticlePreset(installed.Find(id)->particles);
    astra::asset::MeshAsset cube = astra::asset::MakeCube();
    cube.name = "Script Cube";
    const std::uint32_t meshId = installed.AddMesh(std::move(cube));
    CHECK(meshId >= 1);
    installed.Find(id)->mesh.shape = astra::scene::MeshShape::Custom;
    installed.Find(id)->mesh.meshId = meshId;
    installed.Find(id)->mesh.size = astra::scene::Vec3(0.5f, 2.0f, 0.5f);

    const std::string json = installed.ToJson();
    astra::scene::Scene restored;
    std::string error;
    CHECK(restored.FromJson(json, &error));
    CHECK_EQ(restored.Count(), installed.Count());
    CHECK_EQ(restored.Meshes().size(), installed.Meshes().size());
    const astra::scene::Entity* back = restored.Find(id);
    CHECK(back != nullptr);
    if (back) {
        CHECK(back->material.shader == astra::scene::MaterialShader::Hologram);
        CHECK(std::abs(back->material.opacity - 0.6f) < 0.001f);
        CHECK(back->particles.preset == astra::scene::ParticlePreset::Fire);
        CHECK(back->mesh.shape == astra::scene::MeshShape::Custom);
        CHECK_EQ(back->mesh.meshId, meshId);
        CHECK(std::abs(back->mesh.size.y - 2.0f) < 0.001f);
    }
}

void TestVolumetrics() {
    astra::scene::Scene level = astra::scene::Scene::CreateSampleLevel();
    astra::render::TraceCache cache;
    const astra::render::TraceScene& baked = astra::render::GetTraceScene(level, cache);
    CHECK_EQ(cache.builds, 1);

    // Scripted atmosphere control drives the same value the tracer reads.
    {
        astra::script::Host host(level);
        const astra::script::ScriptRun run = host.Run(R"(
            setAtmosphere(0.5, 0.2, 0.0)
            let fog = fogDensity()
            print(fog)
        )");
        CHECK(run.result.ok);
        CHECK(!run.output.empty() && run.output.front().find("0.5") != std::string::npos);
    }
    astra::render::GetTraceScene(level, cache);
    CHECK_EQ(cache.builds, 2);
    CHECK(baked.TriangleCount() > 0);

    // Same content -> same bake, no rebuild.
    CHECK_EQ(level.ContentHash(), level.ContentHash());
    astra::render::GetTraceScene(level, cache);
    CHECK_EQ(cache.builds, 2);

    // Moving an actor changes the hash and forces a rebuild.
    const std::uint64_t before = level.ContentHash();
    if (astra::scene::Entity* cube = level.FindByName("Cube")) {
        cube->transform.position.x += 1.5f;
    }
    CHECK(level.ContentHash() != before);
    astra::render::GetTraceScene(level, cache);
    CHECK_EQ(cache.builds, 3);
    astra::render::Camera camera;
    camera.target = astra::scene::Vec3(0.0f, 1.0f, 0.0f);
    camera.distance = 13.0f;
    camera.yawDeg = 24.0f;
    camera.pitchDeg = 16.0f;

    astra::render::TraceSettings settings;
    settings.width = 160;
    settings.height = 90;
    settings.samples = 8;
    settings.maxBounces = 2;
    settings.denoise = false;

    // The trace scene must pick the fog density up from the level's atmosphere actor.
    float atmosphereFog = 0.0f;
    for (const astra::scene::Entity& entity : level.Entities()) {
        if (entity.Has(astra::scene::Component::Atmosphere)) {
            atmosphereFog = entity.atmosphere.fogDensity;
        }
    }
    CHECK(atmosphereFog > 0.0f);
    // The tracer converts the world's fog amount into a per-metre extinction coefficient.
    const float expectedDensity = atmosphereFog * 0.05f;
    const astra::render::TraceScene& foggy = astra::render::GetTraceScene(level, cache);
    CHECK(std::fabs(foggy.FogDensity() - expectedDensity) < 0.001f);

    settings.volumetrics = false;
    astra::render::TraceStats clearStats;
    const auto clear = astra::render::Render(foggy, camera, settings, &clearStats);
    CHECK(!clear.Empty());
    CHECK(std::fabs(clearStats.fogDensity - expectedDensity) < 0.001f);

    settings.volumetrics = true;
    astra::render::TraceStats foggedStats;
    const auto fogged = astra::render::Render(foggy, camera, settings, &foggedStats);
    CHECK(!fogged.Empty());

    const auto mean = [](const astra::image::Buffer& buffer) {
        double total = 0.0;
        for (int y = 0; y < buffer.height; ++y) {
            for (int x = 0; x < buffer.width; ++x) {
                const auto color = buffer.GetPixel(x, y);
                total += 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
            }
        }
        return total / static_cast<double>(buffer.width * buffer.height);
    };
    const double clearMean = mean(clear);
    const double foggedMean = mean(fogged);
    CHECK(clearMean > 0.0 && foggedMean > 0.0);
    // Fog adds scattered light: the frame brightens and the difference is clearly visible.
    CHECK(foggedMean > clearMean + 0.01);

    // Fog is not free: the volumetric pass casts its own shadow rays.
    CHECK(foggedStats.rays > clearStats.rays);

    // A level with the fog switched off renders identically either way.
    for (astra::scene::Entity& entity : level.Entities()) {
        if (entity.Has(astra::scene::Component::Atmosphere)) entity.atmosphere.fogDensity = 0.0f;
    }
    astra::render::TraceScene noFog;
    noFog.Build(level);
    CHECK(noFog.FogDensity() <= 0.0001f);
    settings.volumetrics = true;
    const auto withToggle = astra::render::Render(noFog, camera, settings, nullptr);
    settings.volumetrics = false;
    const auto withoutToggle = astra::render::Render(noFog, camera, settings, nullptr);
    CHECK(std::fabs(mean(withToggle) - mean(withoutToggle)) < 0.0005);
}

void TestMaterials() {
    const std::string root = "/tmp/astra-selfcheck-materials";
    astra::fs::CreateDirectories(root);
    for (const std::string& entry : astra::fs::ListDirectory(root)) {
        astra::fs::RemoveFile(astra::paths::Join(root, entry));
    }

    // Round trip: what we write must parse back to the same values.
    astra::asset::MaterialAsset authored;
    authored.name = "Test Copper";
    authored.shader = astra::scene::MaterialShader::Hologram;
    authored.baseColor = astra::scene::Vec3(0.72f, 0.42f, 0.18f);
    authored.accentColor = astra::scene::Vec3(0.12f, 0.06f, 0.02f);
    authored.roughness = 0.31f;
    authored.metallic = 0.94f;
    authored.emissive = 0.6f;
    authored.uvScale = 7.0f;

    const std::string text = astra::asset::SerializeMaterial(authored);
    CHECK(text.find("shader = Hologram") != std::string::npos);
    astra::asset::MaterialAsset parsed;
    std::string error;
    CHECK(astra::asset::ParseMaterial(text, parsed, &error));
    CHECK_EQ(parsed.name, authored.name);
    CHECK(parsed.shader == authored.shader);
    CHECK(std::fabs(parsed.baseColor.x - 0.72f) < 0.01f);
    CHECK(std::fabs(parsed.metallic - 0.94f) < 0.01f);
    CHECK(std::fabs(parsed.uvScale - 7.0f) < 0.01f);

    // Tolerant reader: comments, ':' separators, unknown keys, missing keys.
    astra::asset::MaterialAsset loose;
    CHECK(astra::asset::ParseMaterial("# a comment\nname: Loose\nshader: Stripes\n"
                                      "roughness: 0.2\nfutureKey = whatever\n",
                                      loose, &error));
    CHECK_EQ(loose.name, std::string("Loose"));
    CHECK(loose.shader == astra::scene::MaterialShader::Stripes);
    CHECK(std::fabs(loose.roughness - 0.2f) < 0.01f);
    CHECK(std::fabs(loose.metallic) < 0.001f);  // default kept
    // Values are clamped to sane ranges instead of being rejected.
    CHECK(astra::asset::ParseMaterial("roughness = 9\nopacity = -2\n", loose, &error));
    CHECK(loose.roughness <= 1.0f);
    CHECK(loose.opacity >= 0.0f);
    // Broken files fail with a message instead of silently loading garbage.
    CHECK(!astra::asset::ParseMaterial("roughness\n", loose, &error));
    CHECK(!error.empty());

    // Library: seeds the starter set, loads it, applies to an actor.
    astra::asset::MaterialLibrary library;
    library.Scan(root);
    CHECK(library.All().size() >= astra::asset::StarterMaterials().size());
    CHECK(astra::fs::Exists(astra::paths::Join(root, "Brushed_Steel.astramat")));
    const astra::asset::MaterialAsset* steel = library.Find("Brushed Steel");
    CHECK(steel != nullptr);
    if (steel) CHECK(std::fabs(steel->metallic - 0.9f) < 0.01f);

    astra::scene::Scene scene = astra::scene::Scene::CreateEmptyScene("materials");
    astra::scene::Entity* actor = scene.Find(scene.CreateEntity("Panel"));
    CHECK(astra::asset::MaterialLibrary::ApplyTo(scene, actor->id, "Brushed Steel", library));
    actor = scene.Find(actor->id);
    CHECK(actor != nullptr && actor->Has(astra::scene::Component::Material));
    if (actor) {
        CHECK(actor->material.shader == astra::scene::MaterialShader::Lit);
        CHECK(std::fabs(actor->material.metallic - 0.9f) < 0.01f);
    }
    CHECK(!astra::asset::MaterialLibrary::ApplyTo(scene, actor->id, "No Such Material", library));

    // Editing a file on disk and saving through the library writes the new body.
    if (astra::asset::MaterialAsset* mutableSteel = library.FindMutable("Brushed Steel")) {
        mutableSteel->emissive = 1.25f;
        CHECK(library.Save(*mutableSteel));
    }
    astra::asset::MaterialLibrary reloadedLibrary;
    reloadedLibrary.Scan(root);
    const astra::asset::MaterialAsset* reloadedSteel = reloadedLibrary.Find("Brushed Steel");
    CHECK(reloadedSteel != nullptr);
    if (reloadedSteel) CHECK(std::fabs(reloadedSteel->emissive - 1.25f) < 0.01f);

    // Hot reload: touch the file behind the library's back.
    const std::string path = astra::paths::Join(root, "Brushed_Steel.astramat");
    CHECK(astra::fs::WriteTextFileAtomic(path, "name = Brushed Steel\nshader = Checker\n"
                                               "metallic = 0.4\n"));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    // Force a different timestamp: some filesystems have coarse resolution.
    CHECK(astra::fs::WriteTextFileAtomic(path, "name = Brushed Steel\nshader = Checker\n"
                                               "metallic = 0.4\n"));
    CHECK(reloadedLibrary.ReloadChanged() >= 1);
    const astra::asset::MaterialAsset* hot = reloadedLibrary.Find("Brushed Steel");
    CHECK(hot != nullptr);
    if (hot) {
        CHECK(hot->shader == astra::scene::MaterialShader::Checker);
        CHECK(std::fabs(hot->metallic - 0.4f) < 0.01f);
    }
    // Nothing changed -> nothing reloaded.
    CHECK_EQ(reloadedLibrary.ReloadChanged(), 0);

    // The script surface sees the same library.
    astra::scene::Scene scripted = astra::scene::Scene::CreateEmptyScene("scripted");
    astra::script::Host host(scripted);
    const astra::script::ScriptRun run = host.Run(R"(
        let names = materialNames()
        let has = false
        for (name in names) { if (name == "Brushed Steel") { has = true } }
        let id = spawnActor("Panel", 0, 1, 0)
        applyMaterial(id, "Brushed Steel")
    )");
    CHECK(run.result.ok);
    const astra::scene::Entity* panel = scripted.FindByName("Panel");
    CHECK(panel != nullptr);
    if (panel) CHECK(panel->Has(astra::scene::Component::Material));
}

void TestBlenderBridge() {
    // Start from an empty inbox so the assertions are about this run only.
    const std::string inbox = astra::asset::InboxDirectory();
    for (const std::string& entry : astra::fs::ListDirectory(inbox)) {
        std::remove(astra::paths::Join(inbox, entry).c_str());
    }
    CHECK(astra::asset::ScanInbox().empty());

    // The addon writes OBJ + a JSON sidecar; the engine side must accept that exact shape.
    std::string error;
    CHECK(astra::asset::WriteBridgeJob("bridge_cube", astra::asset::MakeSphere(12, 8), "Hologram",
                                       &error));
    const std::vector<astra::asset::BridgeJob> jobs = astra::asset::ScanInbox();
    CHECK_EQ(jobs.size(), std::size_t(1));
    if (!jobs.empty()) {
        CHECK_EQ(jobs[0].name, std::string("bridge_cube"));
        CHECK_EQ(jobs[0].material, std::string("Hologram"));
        CHECK(!jobs[0].placeholder);
    }

    astra::scene::Scene scene = astra::scene::Scene::CreateEmptyScene("bridge");
    const std::size_t before = scene.Count();
    const std::size_t meshesBefore = scene.Meshes().size();
    const astra::asset::BridgeImportResult result = astra::asset::ImportInbox(scene);
    CHECK_EQ(result.imported, 1);
    CHECK_EQ(result.skipped, 0);
    CHECK_EQ(scene.Count(), before + 1);
    CHECK_EQ(scene.Meshes().size(), meshesBefore + 1);
    CHECK(!result.actorNames.empty());

    const astra::scene::Entity* actor =
        result.actorNames.empty() ? nullptr : scene.FindByName(result.actorNames.front());
    CHECK(actor != nullptr);
    if (actor) {
        CHECK(actor->mesh.shape == astra::scene::MeshShape::Custom);
        CHECK(actor->mesh.meshId >= 1);
        CHECK(actor->material.shader == astra::scene::MaterialShader::Hologram);
        CHECK(actor->tag == "blender");
        const astra::scene::MeshAsset* mesh = scene.Mesh(actor->mesh.meshId);
        CHECK(mesh != nullptr);
        if (mesh) CHECK(!mesh->Empty());
    }

    // Imports are moved aside, so a second scan is empty and a second import is a no-op.
    CHECK(astra::asset::ScanInbox().empty());
    const astra::asset::BridgeImportResult again = astra::asset::ImportInbox(scene);
    CHECK_EQ(again.imported, 0);

    // Placeholder jobs (Blender connected but nothing exported yet) are skipped, not errors.
    std::string placeholder = "{\"name\": \"wip\", \"placeholder\": true}";
    CHECK(astra::fs::WriteTextFileAtomic(astra::paths::Join(inbox, "wip.json"), placeholder));
    std::string obj = "o wip\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    CHECK(astra::fs::WriteTextFileAtomic(astra::paths::Join(inbox, "wip.obj"), obj));
    const astra::asset::BridgeImportResult skipped = astra::asset::ImportInbox(scene, false);
    CHECK_EQ(skipped.imported, 0);
    CHECK_EQ(skipped.skipped, 1);

    // A malformed OBJ is reported, never fatal.
    std::remove(astra::paths::Join(inbox, "wip.json").c_str());
    CHECK(astra::fs::WriteTextFileAtomic(astra::paths::Join(inbox, "wip.obj"),
                                         std::string("this is not an obj\n")));
    const astra::asset::BridgeImportResult broken = astra::asset::ImportInbox(scene, false);
    CHECK_EQ(broken.imported, 0);
    CHECK(!broken.errors.empty());
    std::remove(astra::paths::Join(inbox, "wip.obj").c_str());

    // Status reporting tolerates a missing file and reads the addon's heartbeat.
    CHECK(astra::asset::BlenderStatus() != "");
    CHECK(astra::fs::WriteTextFileAtomic(
        astra::asset::StatusPath(),
        std::string("{\"blender\": \"4.2.1\", \"scene\": \"Level\", \"timestamp\": 12}")));
    const std::string status = astra::asset::BlenderStatus();
    CHECK(status.find("4.2.1") != std::string::npos);
    CHECK(status.find("Level") != std::string::npos);
}

void TestViewport() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int atlasWidth = 0;
    int atlasHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlasWidth, &atlasHeight);
    io.Fonts->SetTexID(static_cast<ImTextureID>(1));
    ImGui::NewFrame();

    astra::scene::Scene level = astra::scene::Scene::CreateSampleLevel();
    astra::render::Rect rect{ImVec2(0.0f, 0.0f), ImVec2(1280.0f, 720.0f)};
    astra::render::Camera camera;
    const astra::scene::Entity* cube = level.FindByName("Cube");
    if (cube) camera.target = cube->transform.position;
    camera.distance = 9.0f;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const int verticesBefore = dl->VtxBuffer.Size;

    astra::render::Options options;
    const astra::render::Stats shaded = astra::render::Draw(
        dl, rect, level, camera, options, astra::scene::kInvalidEntity, 0.0f);
    CHECK(shaded.drawnActors >= 4);
    CHECK(shaded.triangles > 100);
    CHECK(dl->VtxBuffer.Size > verticesBefore);
    CHECK(shaded.milliseconds >= 0.0f);

    // Every view mode renders the same actors without NaNs or assertions.
    for (astra::render::ViewMode mode :
         {astra::render::ViewMode::Unlit, astra::render::ViewMode::Wireframe,
          astra::render::ViewMode::Normals}) {
        options.mode = mode;
        const astra::render::Stats stats = astra::render::Draw(
            dl, rect, level, camera, options, astra::scene::kInvalidEntity, 0.0f);
        CHECK(stats.drawnActors == shaded.drawnActors);
        CHECK(stats.triangles > 0);
    }
    options.mode = astra::render::ViewMode::Shaded;

    // Picking the centre of a framed actor returns that actor.
    if (cube) {
        const astra::scene::EntityId hit =
            astra::render::Pick(rect, level, camera, ImVec2(640.0f, 360.0f));
        CHECK(hit == cube->id);
        // Corners of the viewport are empty space.
        CHECK(astra::render::Pick(rect, level, camera, ImVec2(6.0f, 6.0f)) ==
              astra::scene::kInvalidEntity);
    }

    // Framing a selection keeps the camera finite and centred on the actor.
    if (cube) {
        astra::render::FrameSelection(camera, level, cube->id);
        CHECK(camera.distance > 1.0f && camera.distance < 400.0f);
        CHECK(camera.target.x == cube->transform.position.x);
        CHECK(camera.target.z == cube->transform.position.z);
    }

    // An empty level draws only chrome - no crash, no geometry.
    astra::scene::Scene empty;
    const astra::render::Stats none = astra::render::Draw(
        dl, rect, empty, camera, options, astra::scene::kInvalidEntity, 0.0f);
    CHECK_EQ(none.drawnActors, 0);
    CHECK_EQ(none.triangles, 0);

    // Actor bounds include the mesh size, and roots without a mesh stay small.
    if (cube) {
        astra::scene::Vec3 center{}, extents{};
        CHECK(astra::render::ActorBounds(*cube, center, extents));
        CHECK(extents.x > 0.0f && extents.y > 0.0f && extents.z > 0.0f);
    }

    // Camera navigation stays inside its clamps so the orbit can never flip or tunnel.
    astra::render::Camera nav;
    nav.distance = 12.0f;
    const float yawBefore = nav.yawDeg;
    nav.Orbit(30.0f, 0.0f);
    CHECK(nav.yawDeg != yawBefore);
    nav.Orbit(0.0f, 10000.0f);
    CHECK(nav.pitchDeg <= astra::render::Camera::kMaxPitch + 0.001f);
    nav.Orbit(0.0f, -10000.0f);
    CHECK(nav.pitchDeg >= astra::render::Camera::kMinPitch - 0.001f);
    nav.Zoom(10000.0f);
    CHECK(nav.distance >= astra::render::Camera::kMinDistance - 0.001f);
    nav.Zoom(-10000.0f);
    CHECK(nav.distance <= astra::render::Camera::kMaxDistance + 0.001f);
    const astra::scene::Vec3 targetBefore = nav.target;
    nav.Pan(40.0f, -20.0f, 720.0f);
    CHECK(nav.target.x != targetBefore.x || nav.target.y != targetBefore.y ||
          nav.target.z != targetBefore.z);
    CHECK(std::isfinite(nav.Position().x) && std::isfinite(nav.Position().y) &&
          std::isfinite(nav.Position().z));

    // View mode names line up with the enum so the picker cannot drift.
    int modeCount = 0;
    const char* const* modeNames = astra::render::ViewModeNames(modeCount);
    CHECK(modeCount >= 4);
    CHECK(modeNames != nullptr);
    if (modeNames) {
        CHECK(std::strcmp(modeNames[static_cast<int>(astra::render::ViewMode::Shaded)],
                          "Shaded") == 0);
        CHECK(modeNames[static_cast<int>(astra::render::ViewMode::Wireframe)] != nullptr);
    }

    ImGui::EndFrame();
    ImGui::DestroyContext();
}

void TestScript() {
    astra::scene::Scene level = astra::scene::Scene::CreateSampleLevel();
    astra::script::Host host(level);
    std::vector<std::string> lines;
    host.SetOutputSink([&lines](const std::string& line) { lines.push_back(line); });

    const astra::script::ScriptRun run = host.Run(R"(
        let total = 0
        for (i in range(10)) { total = total + i }
        print("total", total)

        fn fib(n) {
            if (n < 2) { return n }
            return fib(n - 1) + fib(n - 2)
        }
        print("fib", fib(12))

        let names = ["a", "b"]
        names.push("c")
        print("join", join(names, "-"), names.length)

        let mixer = 3
        mixer = mixer * 2 + 1
        print("mixer", mixer, type(mixer), type("text"), type(names))
        print("math", round(sqrt(81)), clamp(12, 0, 5), abs(-4))
        print("str", upper("astra"), substr("viewport", 4, 4), format("{}+{}", 1, 2))
    )");
    CHECK(run.result.ok);
    if (!run.result.ok) std::printf("  script error: %s\n", run.result.error.c_str());
    CHECK(run.result.steps > 0);
    CHECK(run.output.size() >= 6);

    const auto has = [&run](const std::string& needle) {
        for (const auto& line : run.output) {
            if (line.find(needle) != std::string::npos) return true;
        }
        return false;
    };
    CHECK(has("total 45"));
    CHECK(has("fib 144"));
    CHECK(has("join a-b-c 3"));
    CHECK(has("mixer 7 number string array"));
    CHECK(has("math 9 5 4"));
    CHECK(has("str ASTRA port 1+2"));

    // ---- the scene API edits the level in place -----------------------------
    const std::size_t before = level.Count();
    const astra::script::ScriptRun sceneRun = host.Run(R"(
        let id = spawnActor("Scripted pillar", "cube", -8.0, 1.0, -2.0)
        setColor(id, 0.9, 0.3, 0.2)
        setScale(id, 1.0, 3.0, 1.0)
        setMaterial(id, "stripes", 0.2, 0.8, 0.0)
        let pos = position(id)
        print("spawned", id, pos[0], pos[1], pos[2])
        print("found", findActor("Scripted pillar") == id)
    )");
    CHECK(sceneRun.result.ok);
    if (!sceneRun.result.ok) std::printf("  scene script error: %s\n", sceneRun.result.error.c_str());
    CHECK_EQ(level.Count(), before + 1);
    const astra::scene::Entity* pillar = level.FindByName("Scripted pillar");
    CHECK(pillar != nullptr);
    if (pillar) {
        CHECK(pillar->transform.scale.y == 3.0f);
        CHECK(pillar->Has(astra::scene::Component::Material));
        CHECK(pillar->material.shader == astra::scene::MaterialShader::Stripes);
    }

    // ---- mesh editing: build, deform, register, attach ----------------------
    const astra::script::ScriptRun meshRun = host.Run(R"(
        let mesh = meshSphere(16, 10)
        let count = meshVertexCount(mesh)
        for (i in range(count)) {
            let v = meshVertex(mesh, i)
            meshSetVertex(mesh, i, v[0] * 1.5, v[1] * 0.6 + sin(v[0] * 6.0) * 0.2, v[2] * 1.5)
        }
        meshRecomputeNormals(mesh)
        let id = meshRegister(mesh, "Scripted blob")
        print("registered", id, meshVertexCount(mesh), meshTriangleCount(mesh))
        let actor = spawnActor("Blob", "custom", 6.0, 1.5, -4.0)
        attachMesh(actor, id)
        setColor(actor, 0.35, 0.85, 0.75)
    )");
    CHECK(meshRun.result.ok);
    if (!meshRun.result.ok) std::printf("  mesh script error: %s\n", meshRun.result.error.c_str());
    const astra::scene::Entity* blob = level.FindByName("Blob");
    CHECK(blob != nullptr);
    if (blob) {
        CHECK(blob->mesh.shape == astra::scene::MeshShape::Custom);
        CHECK(blob->mesh.meshId != 0);
        const astra::scene::MeshAsset* asset = level.Mesh(blob->mesh.meshId);
        CHECK(asset != nullptr);
        if (asset) {
            CHECK(asset->vertices.size() > 100);
            CHECK(asset->TriangleCount() > 100);
            CHECK(asset->name == "Scripted blob");
        }
    }

    // ---- image editing: draw, save, reload ---------------------------------
    const astra::script::ScriptRun imageRun = host.Run(R"(
        let img = imageNew(48, 32)
        imageGradient(img, 0.05, 0.08, 0.14, 0.9, 0.35, 0.30)
        imageCircle(img, 24, 16, 10, 0.2, 0.8, 0.9, 1.0)
        imageRect(img, 2, 2, 6, 4, 1.0, 1.0, 1.0)
        imageNoise(img, 0.05)
        imageSave(img, "/tmp/astra-selfcheck-image.png")
        print("image", imageWidth(img), imageHeight(img), imageGet(img, 24, 16)[2])
    )");
    CHECK(imageRun.result.ok);
    if (!imageRun.result.ok) std::printf("  image script error: %s\n", imageRun.result.error.c_str());
    CHECK(host.Images().size() == 1);
    if (!host.Images().empty()) {
        CHECK_EQ(host.Images()[0].width, 48);
        CHECK_EQ(host.Images()[0].height, 32);
    }
    astra::image::Buffer reloaded;
    CHECK(astra::image::ReadPng("/tmp/astra-selfcheck-image.png", reloaded));
    CHECK_EQ(reloaded.width, 48);
    CHECK_EQ(reloaded.height, 32);

    // ---- diagnostics -------------------------------------------------------
    const astra::script::ScriptRun missing = host.Run("noSuchFunction(1)");
    CHECK(!missing.result.ok);
    CHECK(!missing.result.error.empty());
    CHECK(missing.result.line >= 1);

    const astra::script::ScriptRun syntax = host.Run("let x = ;");
    CHECK(!syntax.result.ok);
    CHECK(!syntax.result.error.empty());

    const astra::script::ScriptRun typeError = host.Run("let x = 1\nx.foo = 2");
    CHECK(!typeError.result.ok);

    // The step budget stops runaway loops instead of freezing the editor.
    astra::script::Host limited(level);
    limited.SetStepLimit(2000);
    const astra::script::ScriptRun runaway = limited.Run("while (true) { }");
    CHECK(!runaway.result.ok);
    CHECK(runaway.result.error.find("budget") != std::string::npos);
}

void TestProjects() {
    using namespace astra::project;
    const std::string root = astra::paths::EnsureDataDir("tests/projects");
    const std::string name = "Self Check Project " + astra::crypto::RandomHex(3);

    ProjectInfo info;
    std::string error;
    CHECK(Create(root, name, info, &error));
    CHECK(!error.empty() == false);
    CHECK(info.isValid());
    CHECK(IsProjectDirectory(info.rootPath));

    ProjectInfo readBack;
    CHECK(Read(info.rootPath, readBack));
    CHECK_EQ(readBack.name, info.name);

    astra::scene::Scene scene = astra::scene::Scene::CreateStormScene("Scenes/Wind");
    CHECK(SaveScene(info, scene));
    astra::scene::Scene loaded;
    CHECK(LoadScene(info, loaded));
    CHECK_EQ(loaded.Name(), std::string("Scenes/Wind"));

    RecentProjects recents(astra::paths::Join(astra::paths::EnsureDataDir("tests"), "recent.json"));
    recents.Touch(info);
    CHECK_EQ(recents.Entries().size(), static_cast<std::size_t>(1));
    recents.Touch(info);
    CHECK_EQ(recents.Entries().size(), static_cast<std::size_t>(1));
}

void TestGitHubParsing() {
    const std::string body = R"([
        {
            "id": 7,
            "name": "supercell-sim",
            "full_name": "astrapictures/supercell-sim",
            "owner": {"login": "astrapictures"},
            "description": "Playable supercell intercept scenario",
            "language": "C++",
            "default_branch": "main",
            "private": true,
            "fork": false,
            "archived": false,
            "stargazers_count": 412,
            "open_issues_count": 3,
            "size": 96422,
            "updated_at": "2026-11-19T03:12:00Z",
            "pushed_at": "2026-11-19T03:12:00Z",
            "topics": ["astra-engine", "storm-chasing"]
        },
        {
            "id": 8,
            "name": "radar-mosaic",
            "full_name": "astrapictures/radar-mosaic",
            "owner": {"login": "astrapictures"},
            "description": "",
            "language": "Python",
            "private": false,
            "stargazers_count": 233,
            "pushed_at": "2026-11-19T07:05:00Z",
            "topics": []
        }
    ])";

    std::vector<astra::github::Repo> repos;
    std::string error;
    CHECK(astra::github::ParseRepoArray(body, repos, &error));
    CHECK_EQ(repos.size(), static_cast<std::size_t>(2));
    if (repos.size() == 2) {
        CHECK_EQ(repos[0].name, std::string("supercell-sim"));
        CHECK(repos[0].isPrivate);
        CHECK(repos[0].hasAstraProject);
        CHECK_EQ(repos[0].stars, 412);
        CHECK(repos[0].pushedUtc > 0);
        CHECK_EQ(repos[0].defaultBranch, std::string("main"));
        CHECK(!repos[1].hasAstraProject);
    }

    // Error envelope.
    std::vector<astra::github::Repo> none;
    CHECK(!astra::github::ParseRepoArray(R"({"message":"Bad credentials"})", none, &error));
    CHECK(astra::str::Contains(error, "Bad credentials"));

    // Link header pagination.
    CHECK_EQ(astra::github::ParseNextPage(
                 "<https://api.github.com/user/repos?page=2>; rel=\"next\", "
                 "<https://api.github.com/user/repos?page=5>; rel=\"last\""),
             2);
    CHECK_EQ(astra::github::ParseNextPage("<https://api.github.com/user/repos?page=1>; rel=\"last\""),
             0);

    astra::github::User user;
    CHECK(astra::github::ParseUser(R"({"login":"astrapictures","name":"Astra","public_repos":14})",
                                   user, &error));
    CHECK_EQ(user.login, std::string("astrapictures"));

    CHECK(astra::github::RepoListUrl(2, 100).find("page=2") != std::string::npos);
}

void TestSettingsRoundTrip() {
    astra::app::AppSettings settings;
    settings.uiScale = 1.15f;
    settings.density = astra::app::UiDensity::Spacious;
    settings.githubLogin = "astrapictures";
    settings.recentProjects = {"/tmp/a", "/tmp/b"};
    settings.showViewportPlaceholder = false;

    const std::string text = settings.Serialize();
    astra::app::AppSettings restored = astra::app::AppSettings::Deserialize(text);
    CHECK_EQ(restored.githubLogin, std::string("astrapictures"));
    CHECK(restored.recentProjects.size() == 2);
    CHECK(restored.density == astra::app::UiDensity::Spacious);
    CHECK(!restored.showViewportPlaceholder);

    restored.PushRecentProject("/tmp/b");
    CHECK_EQ(restored.recentProjects.front(), std::string("/tmp/b"));
}

struct TestCase {
    const char* name;
    void (*fn)();
};

const TestCase kTests[] = {
    {"crypto.sha256", TestSha256Vectors},
    {"crypto.hmac", TestHmac},
    {"crypto.pbkdf2", TestPbkdf2},
    {"crypto.base32", TestBase32},
    {"data.json", TestJson},
    {"licensing.codes", TestAccessCodes},
    {"auth.accounts", TestAccounts},
    {"auth.registration", TestRegistrationFlow},
    {"scene.runtime", TestScene},
    {"scene.sample_level", TestSampleLevel},
    {"render.viewport", TestViewport},
    {"render.pathtracer", TestPathtracer},
    {"scene.presets", TestPresets},
    {"game.player", TestPlayer},
    {"bridge.blender", TestBlenderBridge},
    {"render.volumetrics", TestVolumetrics},
    {"asset.materials", TestMaterials},
    {"script.language", TestScript},
    {"project.files", TestProjects},
    {"github.parsing", TestGitHubParsing},
    {"settings.roundtrip", TestSettingsRoundTrip},
};

}  // namespace

int main(int argc, char** argv) {
    std::string filter;
    bool listOnly = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--filter" && i + 1 < argc) filter = argv[++i];
        if (arg == "--list") listOnly = true;
    }

    // Tests are hermetic: start from an empty data root every run.
    std::error_code ec;
    std::filesystem::remove_all("/tmp/astra-selfcheck-data", ec);
    astra::paths::SetDataRootOverride("/tmp/astra-selfcheck-data");
    astra::crypto::SetDeterministicSeedForTesting(0x5EED);

    if (listOnly) {
        for (const auto& test : kTests) std::printf("%s\n", test.name);
        return 0;
    }

    std::printf("Astra self-check\n----------------\n");
    int ran = 0;
    for (const auto& test : kTests) {
        if (!filter.empty() && std::strstr(test.name, filter.c_str()) == nullptr) continue;
        ++ran;
        gCurrentTest = test.name;
        const int before = gFailures;
        const int checksBefore = gChecks;
        test.fn();
        const int checks = gChecks - checksBefore;
        const bool passed = gFailures == before;
        std::printf("%-24s %s  (%d checks)\n", test.name, passed ? "PASS" : "FAIL", checks);
    }

    std::printf("----------------\n%d test(s), %d check(s), %d failure(s)\n", ran, gChecks,
                gFailures);
    return gFailures == 0 ? 0 : 1;
}
