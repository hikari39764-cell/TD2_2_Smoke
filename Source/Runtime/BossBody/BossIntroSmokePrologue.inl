// 登場の煙（c-e0c500）のうち、序幕の黒い煙（Ambient）。BossIntroSmoke.cpp の無名の名前空間の中へ、そのまま取り込む
// （1 ファイル 800 行以内に収めるために分けた。Feed・Wisp・CueLocal・Passed と形の決まりは、取り込む側が先に用意する）。

// 煙の筋が巡る所 1 つ：決まった席（番号を席で決める：部品が減っても、ほかの所の点の番号は変わらない）、地面の点、全体の
// 量のうちの取り分、巡る輪の半径 (m)、回る向き（+1 か -1。隣り合う所を逆に回す）、筋の本数。
struct AmbientSite {
    int slot = 0;
    Vec3 at{};
    float share = 0.0f;
    float reach = 1.0f;
    float turn = 1.0f;
    int streams = 1;
};

// 煙の筋が巡る所（鎧・刀の根元・兜・右の手甲・左の手甲。部品は、まだ体に付いていないものだけ）。数を返す。先頭は鎧。
int AmbientPlaces(const BossIntro& intro, std::array<AmbientSite, kAmbientSites>& sites) {
    const BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    if (!a.socketsValid)
        return 0;
    int count = 0;
    sites[static_cast<size_t>(count++)] = {0, Ground(state, Lerp(a.hips.position, a.chest.position, 0.5f)), 0.30f, 2.3f,
                                           1.0f, 2};
    sites[static_cast<size_t>(count++)] = {1, Ground(state, BossIntroActor::SwordEntry(intro)), 0.20f, 1.5f, -1.0f, 1};
    struct Part {
        BossIntroPart part;
        float share;
        float reach;
        float turn;
    };
    constexpr std::array<Part, 3> parts{{{BossIntroPart::Helm, 0.18f, 1.7f, 1.0f},
                                         {BossIntroPart::GauntletRight, 0.16f, 1.3f, -1.0f},
                                         {BossIntroPart::GauntletLeft, 0.16f, 1.3f, 1.0f}}};
    for (size_t k = 0; k < parts.size(); ++k) {
        const size_t index = static_cast<size_t>(parts[k].part);
        if (a.attached[index] || a.partShown[index] != 1)
            continue;
        sites[static_cast<size_t>(count++)] = {2 + static_cast<int>(k), Ground(state, a.partAt[index]), parts[k].share,
                                               parts[k].reach, parts[k].turn, 1};
    }
    return count;
}

// 序幕の黒い煙：演技の前（プレイヤーを待つ間）から、濃い黒い煙の筋が、部品の周りをそれぞれ巡り続ける。小さな点が部品の
// 周りの大きな輪の上を速く走り（エンジンが点の動いた跡へ掃いて入れる）、後ろに尾を引く：1 周する前に尾が薄れるので、輪は
// 閉じず、彗星のような筋が部品を巡る形になる（点を増やしたり遅くしたりすると、尾がつながって、その場に溜まった丸い塊に
// 見える。薄く広げると、形の無い濁りになる）。部品そのものは輪の中にあって、煙に隠れない。前触れ（拍 Omen）が進むと、筋は
// 自分の部品を離れて鎧へ寄り、鎧の周りを巡る渦になる。地を走る煙が鎧に着く前に引いていく（拍 Smoke.Ambient の終わり）。
void Ambient(const Feed& feed, const BossIntro& intro) {
    const BossIntro& s = intro;
    const BossIntroState& state = intro.state;
    if (s.introAmbientAmount <= 0.0f)
        return;
    float level = Smooth(state.ambientClock / kAmbientIn);
    float omen = 0.0f;
    if (state.phase == BossIntroPhase::Show) {
        float duration = 0.0f;
        const float local = CueLocal(state, Kind::SmokeAmbient, duration);
        if (local < 0.0f)
            return;
        level *= Smooth((duration - local) / kAmbientOut);
        float length = 0.0f;
        const float since = CueLocal(state, Kind::Omen, length);
        omen = since >= 0.0f ? Smooth(since / length) : Passed(state, Kind::Omen) ? 1.0f : 0.0f;
    }
    std::array<AmbientSite, kAmbientSites> sites{};
    const int count = AmbientPlaces(intro, sites);
    if (level <= 0.0f || count == 0)
        return;
    const Vec3 armour = sites[0].at;
    const float time = state.ambientClock;
    for (int k = 0; k < count; ++k) {
        const AmbientSite& site = sites[static_cast<size_t>(k)];
        const float seat = static_cast<float>(site.slot);
        // 巡る中心と半径：ふだんは自分の部品の周り（半径はゆっくり伸び縮みする）。前触れが進むと、部品を離れて鎧の周りへ
        // 寄って行く（鎧の周りでは、席ごとに半径を変えて、筋を重ねない）。
        const float home = std::max(kAmbientRingMin, site.reach * s.introAmbientRing);
        const Vec3 centre = Lerp(site.at, armour, omen);
        const float radius = Lerp(home * (1.0f + kAmbientBreathe * std::sin(time * 0.37f + 1.7f * seat)),
                                  kOmenRing + kOmenRingStep * seat, omen);
        // 角度は「時刻 × (速さ ÷ ふだんの半径)」で決める：半径や中心が変わっても、点は跳ばない。
        const float rate = site.turn * s.introAmbientTurn / home;
        for (int w = 0; w < site.streams; ++w) {
            const uint64_t id = kAmbientId + static_cast<uint64_t>(site.slot * kAmbientWisps + w);
            const Vec3 out = Around(2.1f * seat + time * rate +
                                    2.0f * kPi * static_cast<float>(w) / static_cast<float>(site.streams));
            Wisp wisp{};
            wisp.at = Add(centre, Add(Mul(out, radius), Up(0.3f + 0.08f * std::sin(time * 0.9f + seat))));
            wisp.radius = 0.36f;
            wisp.velocity = Add(Mul(Tangent(out), site.turn * kAmbientCarry), Up(0.15f));
            wisp.gust = 0.3f;
            wisp.heat = Lerp(s.introAmbientHeat, kOmenHeat, omen);
            feed.Smoke(id, wisp, s.introAmbientAmount * site.share * level / static_cast<float>(site.streams));
        }
        // 輪を回す力：所の周りをゆっくり巡りながら、筋と同じ向きへ押す（尾が輪に沿って流れ続ける）。前触れでは強まり、
        // 鎧の周りの渦を回す。
        const Vec3 arm = Around(1.3f * seat + time * rate * kAmbientStirShare);
        feed.Blow(kAmbientId + 16 + static_cast<uint64_t>(site.slot), Add(centre, Add(Mul(arm, radius * 0.6f), Up(0.5f))),
                  radius + 0.8f, Mul(Tangent(arm), site.turn), Lerp(s.introAmbientStir, s.introOmenPull, omen) * level,
                  FluidChannels::kIntroSmoke);
        // 尾を薄める：この域の煙は長く残る設定（抜刀の柱を立ち残らせるため）なので、放っておくと、巡る筋の尾が 1 周して
        // つながり、溜まって丸い塊になる。巡る輪の一帯で薄め続けて、尾を 1 周より短く保つ（前触れで筋が鎧へ集まる時は、
        // 重なる分だけ弱める）。
        feed.Absorb(kAmbientId + 30 + static_cast<uint64_t>(site.slot), Add(centre, Up(0.5f)),
                    radius * kAmbientFadeReach + 1.0f,
                    s.introAmbientFade * level * Lerp(1.0f, 1.0f / static_cast<float>(count), omen));
        if (s.introAmbientFogPush > 0.0f)
            feed.Burst(kAmbientId + 24 + static_cast<uint64_t>(site.slot), site.at, kAmbientFogRadius,
                       s.introAmbientFogPush * level, FluidChannels::kFog);
    }
}
