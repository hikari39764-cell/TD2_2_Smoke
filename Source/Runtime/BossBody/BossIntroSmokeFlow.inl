// 登場の煙（c-e0c500）のうち、刀の根元の煙（Seep・SlitSmoke）と、流れをつくる 3 つ：握る間に根元へ集まる霧（Gather）、見得の黒い霧（Backdrop）、抜けた瞬間に
// 根元から走る煙の筋（DrawStreamers）、見得の終わりに霧を晴らす流れ（Disperse）。BossIntroSmoke.cpp の無名の名前空間の中へ、そのまま取り込む（1 ファイル 800 行
// 以内に収めるために分けた。Feed・Wisp・CueLocal・Since と形の決まりは、取り込む側が先に用意する）。

// 剣先の寄りの段がある演出の、地の割れ目の向き：刀の幅の向き（刃口の向き）を地面へ寝かせる。
Vec3 SlitAlong(const BossIntroComponent& intro) {
    BossBodyPose pose{};
    const Vec3 across = Tangent(Forward(intro.state.yaw));
    return BossIntroActor::Pose(intro, pose) ? FlatNormalize(Rotate(pose.sword.rotation, {0.0f, -1.0f, 0.0f}), across)
                                             : across;
}

// 地の割れ目（刀が刺さった所）から出る煙。mass＝1 秒に入れる煙の総量。点は地面の下に沈めてあり、域へ入るのは地面の
// すぐ上の細い線だけ（球の形は見えない）。寄りの間の滲み出しも、抜けた瞬間に溢れる煙も、同じ点から出す。
void SlitSmoke(const Feed& feed, const BossIntroComponent& intro, float mass) {
    const BossIntroState& state = intro.state;
    const Vec3 entry = BossIntroActor::SwordEntry(intro);
    const Vec3 slit = SlitAlong(intro);
    const int count = kSeepGround + kSeepBlade;
    for (int k = 0; k < count; ++k) {
        const uint64_t id = kSeepId + static_cast<uint64_t>(k);
        const bool main = k < kTipSlitMain;
        const float turn = main ? 0.0f : kTipSlitSideAngle[k - kTipSlitMain] * kRadiansPerDegree;
        const Vec3 along = Add(Mul(slit, std::cos(turn)), Mul(Tangent(slit), std::sin(turn)));
        // 割れ目の上を往復する（点ごとに速さと位相を変える。正弦なので、端で止まらない）。斜めの割れ目は根元から先へだけ。
        const float swing = std::sin(state.showSeconds * kTipSlitSlide * (1.0f + 0.23f * static_cast<float>(k)) +
                                     1.9f * static_cast<float>(k));
        const float offset = main ? kTipSlitHalf * swing : kTipSlitSideLength * (0.55f + 0.45f * swing);
        Wisp wisp{};
        wisp.at = Add(entry, Add(Mul(along, offset), Up(-kTipSlitDepth)));
        wisp.spread = 0.04f;
        wisp.radius = kWispRadiusMin;
        wisp.heat = kTipSeepHeat;
        feed.Smoke(id, wisp, mass / static_cast<float>(count));
    }
}

// 割れ目の上の空気を温める（煙はほとんど足さない）。滲み出した煙が、巻きながら自分で立ちのぼる。
void SlitWarm(const Feed& feed, const BossIntroComponent& intro) {
    const Vec3 entry = BossIntroActor::SwordEntry(intro);
    const Vec3 slit = SlitAlong(intro);
    for (uint64_t k = 0; k < 2u; ++k)
        feed.Warm(kTipWarmId + k,
                  Add(entry, Add(Mul(slit, (k == 0u ? 1.0f : -1.0f) * kTipWarmAside), Up(kTipWarmHeight))),
                  kTipWarmRadius, kTipSeepHeat);
}

// 刀の根元の細かい煙の域（BossIntroTipSmoke）を、握り始めから抜けた直後まで、刀が地面に入る所へ置く（剣先の寄りの
// カットの間だけ見える。項目が空・段が無い演出では置かない）。Render から毎フレーム。
void TipDomain(HIKARI::WorldView& world, BossComponent& boss, BossIntroComponent& intro) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    float duration = 0.0f;
    const float drawn = Since(state, Kind::SwordOut);
    const bool wanted = state.phase == BossIntroPhase::Show && !state.skipped && !s.tipSmokeEffect.IsEmpty() &&
                        Has(state, Kind::SwordTip) && drawn < kTipFineLinger &&
                        (drawn >= 0.0f || CueLocal(state, Kind::SwordGrab, duration) >= 0.0f ||
                         CueLocal(state, Kind::SwordTip, duration) >= 0.0f);
    if (!wanted) {
        if (state.tipSmokeHandle.IsValid())
            BossVfx::StopHandle(world, state.tipSmokeHandle);
        return;
    }
    if (!state.tipSmokeHandle.IsValid()) {
        // 域の床は地面に合わせる：舞台の原点の高さに置くと、地形がそこより高い所では、煙が地面の中に隠れる。刀が入る
        // 所の真下へ光線を落として、実際の地面を取る（取れなければ、原点の高さ）。
        const Vec3 entry = BossIntroActor::SwordEntry(intro);
        state.tipGround = entry;
        HIKARI::GameProjectPhysicsRaycastQuery query{};
        query.origin = Add(entry, Up(kTipGroundAbove));
        query.direction = {0.0f, -1.0f, 0.0f};
        query.maxDistance = kTipGroundReach;
        query.filter.includeTriggers = 0;
        HIKARI::GameProjectPhysicsHit hit{};
        const bool found = world.GetPhysicsQueries().Raycast(query, hit);
        if (found)
            state.tipGround.y = hit.position.y;
        char message[128]{};
        std::snprintf(message, sizeof(message), "IntroTipSmoke ground found=%d y=%.3f origin=%.3f",
                      static_cast<int>(found), static_cast<double>(state.tipGround.y),
                      static_cast<double>(state.origin.y));
        BossLog::Write(message);
    }
    BossVfx::StartOrSend(world, boss, state.tipSmokeHandle, s.tipSmokeEffect,
                         BossVfx::Placed(state.tipGround, 1.0f, YawRotation(state.yaw)), {});
}

// 手を握る拍（Hand.Fist）：右手を握り込むたびに、指の間（付け根の間 3 か所と、親指の股）から、黒い煙が細く押し出され、
// 握っている間は細く漏れ続ける。握り込む勢いは、役者の指の骨から読む（手首から中指の先までが縮む速さ）：動画の握る所に、
// 自分で合う。煙は、手の所に置く細かい域（BossIntroFistSmoke）へ入れる。Render から毎フレーム。
void FistSmoke(HIKARI::WorldView& world, const Feed& feed, BossComponent& boss, BossIntroComponent& intro,
               float seconds) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::HandFist, duration);
    if (state.phase != BossIntroPhase::Show || state.skipped || s.fistSmokeEffect.IsEmpty() || local < 0.0f ||
        !a.socketsValid || !a.fingersValid) {
        if (state.fistSmokeHandle.IsValid())
            BossVfx::StopHandle(world, state.fistSmokeHandle);
        state.fistCurl = -1.0f;
        state.fistSqueeze = 0.0f;
        return;
    }
    const Vec3 wrist = a.rightHand.position;
    Vec3 knuckles{};
    for (const HIKARI::GameProjectTransform& knuckle : a.rightKnuckles)
        knuckles = Add(knuckles, Mul(knuckle.position, 0.25f));
    const float curl = Length(Sub(a.rightFingerTip.position, wrist));
    if (!state.fistSmokeHandle.IsValid()) {
        const Vec3 forward = Forward(state.yaw);
        state.fistAt = Add(knuckles, Add(Add(Mul(Tangent(forward), kFistDomainSide), Mul(forward, -kFistDomainBack)),
                                         Up(kFistDomainUp)));
        state.fistOpen = curl;
        state.fistCurl = curl;
    }
    BossVfx::StartOrSend(world, boss, state.fistSmokeHandle, s.fistSmokeEffect,
                         BossVfx::Placed(state.fistAt, 1.0f, YawRotation(state.yaw)), {});
    // 握り込む勢い：ならした長さが縮む速さから。速く立ち上がり、ゆっくり引く（押し出された煙が、尾を引いて細る）。
    const float dt = std::max(0.0f, seconds);
    const float before = state.fistCurl;
    state.fistCurl += (curl - state.fistCurl) * std::clamp(kFistCurlFollow * dt, 0.0f, 1.0f);
    const float closing = dt > 0.0f ? std::max(0.0f, (before - state.fistCurl) / dt) : 0.0f;
    const float want = std::clamp(closing / kFistClosingFull, 0.0f, 1.0f);
    state.fistSqueeze += (want - state.fistSqueeze) *
                         std::clamp((want > state.fistSqueeze ? kFistSqueezeRise : kFistSqueezeFall) * dt, 0.0f, 1.0f);
    const float held = Smooth((state.fistOpen - state.fistCurl) / kFistHeldDepth);
    const float fade = Smooth((local - (duration - kFistFadeSeconds)) / kFistFadeSeconds);
    const float rate =
        kFistSmokeRate * s.fistSmokeAmount * (state.fistSqueeze + kFistSmokeTrickle * held) * (1.0f - fade);
    const Vec3 centre = Lerp(wrist, knuckles, 0.6f);
    for (int k = 0; k < 4; ++k) {
        const uint64_t id = kFistSmokeId + static_cast<uint64_t>(k);
        // 付け根どうしの間 3 か所と、親指と人差し指の股。
        const Vec3 gap = k < 3 ? Lerp(a.rightKnuckles[static_cast<size_t>(k)].position,
                                      a.rightKnuckles[static_cast<size_t>(k) + 1u].position, 0.5f)
                               : Lerp(a.rightThumb.position, a.rightKnuckles[0].position, 0.5f);
        // 押し出す向き：ほぼ真上へ（拳の上は空：暗い鎧の前へ流すと、煙が読めない。腕に沿って垂れもしない）。
        const Vec3 outward = Normalize(Add(Mul(Normalize(Sub(gap, centre), {0.0f, 1.0f, 0.0f}), 0.25f), Up(1.0f)));
        feed.Send(id, Fluid::Emit, Add(gap, Mul(feed.Jitter(id, 21u), 0.02f)), {kFistSmokeRadius, kFistSmokeHeat, 0.0f},
                  Mul(outward, kFistSmokeSpeed * (0.3f + state.fistSqueeze)), rate * (0.5f + 0.5f * feed.Roll(id, 6u)),
                  0.0f, FluidChannels::kIntroTip);
    }
    // 拍の終わり：域を止める前に、残った煙を薄め切る（止めた瞬間に、煙が消えて見えない）。
    if (fade > 0.0f)
        feed.Send(kFistSmokeId + 4, Fluid::Force, state.fistAt, {3.0f, 0.0f, 0.0f}, {}, 0.0f, kFistFadeRate * fade,
                  FluidChannels::kIntroTip);
}

// 細かい域へ滲み出す煙：割れ目の線の上を滑る小さな点から入れる（x＝段の進み 0〜1）。出始めが速く、ゆるやかに濃くなり、
// 刀が抜けて行く終わりの所で、もう一段濃くなる。点ごとに量をゆっくり変える（割れ目のあちこちから、不揃いに滲む）。
// 温かいので、巻きながら自分で立ちのぼり、冷めると漂う。
void SeepFine(const Feed& feed, const BossIntroComponent& intro, float x) {
    const BossIntroState& state = intro.state;
    const Vec3 entry = state.tipGround;
    const Vec3 slit = SlitAlong(intro);
    const float rate = kTipFineRate * intro.settings.tipSmokeAmount * (EaseOut(x) + kTipFineSurge * x * x * x * x);
    for (int k = 0; k < kTipFinePoints; ++k) {
        const uint64_t id = kTipFineId + static_cast<uint64_t>(k);
        const bool main = k < kTipFineMain;
        const float turn = main ? 0.0f : kTipSlitSideAngle[(k - kTipFineMain) % 2] * kRadiansPerDegree;
        const Vec3 along = Add(Mul(slit, std::cos(turn)), Mul(Tangent(slit), std::sin(turn)));
        const float swing = std::sin(state.showSeconds * kTipFineSlide * (1.0f + 0.17f * static_cast<float>(k)) +
                                     2.3f * static_cast<float>(k));
        const float offset = main ? kTipSlitHalf * swing : kTipSlitSideLength * (0.55f + 0.45f * swing);
        const Vec3 at = Add(entry, Add(Mul(along, offset), Up(kTipFineHeight)));
        const Vec3 velocity = Add(Up(kTipFineRise), Mul(feed.Jitter(id, 17u), 0.25f));
        feed.Send(id, Fluid::Emit, at, {kTipFineRadius, kTipFineHeat, 0.0f}, velocity,
                  rate * (0.35f + 0.65f * feed.Roll(id, 6u)), 0.0f, FluidChannels::kIntroTip);
    }
}

// 剣先の寄りの段がある演出の風：刀のある側の外へ（back＝混ぜる「後ろ」の割合）、根元の低い所の力と、舞台じゅうの広い
// 力で吹く（low・wide＝加速 m/秒²）。向きはゆっくり左右へ振れる。煙はその場に立ち続けず、風下へ流れて、ほぐれて広がる。
void TipWind(const Feed& feed, const BossIntroComponent& intro, float low, float wide, float back,
             uint32_t channels = FluidChannels::kIntroSmoke) {
    const BossIntroState& state = intro.state;
    const Vec3 entry = BossIntroActor::SwordEntry(intro);
    const Vec3 feet = BossIntroSmoke::FeetWorld(intro);
    const Vec3 forward = Forward(state.yaw);
    const Vec3 body = FlatNormalize(Sub(feet, entry), Mul(forward, -1.0f));
    const Vec3 outside = FlatNormalize(ProjectOut(Mul(body, -1.0f), forward), Tangent(forward));
    const Vec3 mean = FlatNormalize(Add(outside, Mul(forward, -back)), outside);
    for (int k = 0; k < 2; ++k) {
        const bool root = k == 0;
        const float push = root ? low : wide;
        if (push <= 0.0f)
            continue;
        const float sway = kTipWindSway * std::sin(state.showSeconds * kTipWindSwaySpeed + (root ? 0.0f : 1.9f));
        const Vec3 along = Add(Mul(mean, std::cos(sway)), Mul(Tangent(mean), std::sin(sway)));
        feed.Blow(kTipWindId + static_cast<uint64_t>(k),
                  root ? Add(entry, Add(Mul(mean, kTipWindLowAhead), Up(kTipWindLowHeight)))
                       : Add(feet, Up(kTipWindWideHeight)),
                  root ? kTipWindLowRadius : kTipWindWideRadius, Normalize(Add(along, Up(root ? 0.05f : 0.12f))), push,
                  channels);
    }
}

// 剣先の寄りの段がある演出の抜刀（Draw から）：滲み出していた割れ目から煙が一気に溢れ、風に運ばれて散り、見得の黒い霧
// （Backdrop）へ続く。柱は立てない（噴き上げる力と煙突は使わない：その場に柱で残って、広がって見えない）。
// root＝根元、outside＝刀のある側の外、forward＝舞台の前、local＝抜けてからの秒。
void DrawSpill(const Feed& feed, const BossIntroComponent& intro, Vec3 root, Vec3 outside, Vec3 forward,
               float local) {
    const BossIntroSettings& s = intro.settings;
    const float burst = s.drawBurstSeconds * kTipBurstShare;
    // 風の向き（振れの真ん中）：刀のある側の外へ、後ろ寄りに。
    const Vec3 mean = FlatNormalize(Add(outside, Mul(forward, -kTipWindGustBack)), outside);
    // 抜けた瞬間が最大で、細って行く。割れ目の上は、初めのうち温め続ける（溢れた煙が、盛り上がってから流れる）。
    if (local < burst)
        SlitSmoke(feed, intro, s.drawBurstAmount * kTipSpillGain * (1.0f - Smooth(local / burst)));
    if (local < kTipSpillWarm)
        SlitWarm(feed, intro);
    // 風：寄りの間のそよ風から、抜けた瞬間に突風へ強まり、引いて弱い風が残る。
    const float gust = Smooth(local / kTipWindGustIn);
    const float calm = 1.0f - (1.0f - kTipWindBreeze) * Smooth((local - kTipWindGustHold) / kTipWindGustFall);
    TipWind(feed, intro, Lerp(kTipWindSeep, kTipWindGustLow, gust) * calm, kTipWindGustWide * gust * calm,
            kTipWindGustBack);
    // 湧き出し：割れ目から圧のかかった煙が噴き出す。溢れた煙を、根元から四方へ（地面に沿って、上へ）押し広げる。抜けた
    // 瞬間が最大で、引いて行く。
    if (local < kTipSwellSeconds)
        feed.Swell(kTipWindId + 2, Add(root, Up(kTipSwellHeight)), kTipSwellRadius,
                   kTipSwell * (1.0f - Smooth(local / kTipSwellSeconds)), FluidChannels::kIntroSmoke);
    // 煙が広がる先を、先に開けておく（根元の周りの 4 か所）。
    if (local < kTipOpenSeconds) {
        for (int k = 0; k < kTipOpen; ++k) {
            const float turn = 2.0f * kPi * (static_cast<float>(k) + 0.5f) / static_cast<float>(kTipOpen);
            const Vec3 away = Add(Mul(mean, std::cos(turn)), Mul(Tangent(mean), std::sin(turn)));
            feed.Warm(kWarmId + static_cast<uint64_t>(k), Add(root, Add(Mul(away, kTipOpenAway), Up(kTipOpenHeight))),
                      kTipOpenRadius, kTipOpenHeat);
        }
    }
    // 域の縁の手前で薄める（流した先と、上）：煙が域の端まで届くと、そこで断ち切られて見える。
    if (local >= kTipEdgeAt) {
        feed.Absorb(kClearId + 3, Add(root, Add(Mul(mean, kBurstEdgeAhead), Up(4.0f))), kBurstEdgeRadius,
                    kBurstEdgeRate);
        feed.Absorb(kClearId + 4, Add(root, Up(kBurstEdgeAbove)), kBurstEdgeRadius, kBurstEdgeRate);
    }
    // 溢れ終わったら、根元の所を少しずつ薄める（最後のカットで、主役の前を塞がない）。
    if (local >= burst)
        feed.Absorb(kClearId + 1, Add(root, Up(2.4f)), kTipFadeRadius,
                    kBurstFadeRate * Smooth((local - burst) / kTipFadeIn));
}

// 刀を握っている間、刀の根元へ地面の煙がゆっくり寄り、刀身に沿って自分から立ち上がる（噴かない。握るほど濃くなる）。
void Seep(const Feed& feed, const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    float duration = 0.0f;
    // 剣先の寄りの段（Sword.Tip）がある演出では、その段に合わせて、地の割れ目から滲み出させる（寄りに切り替わる前は
    // 出さない：寄りの最初の画は、刀の根元がまだ見える）。段が無ければ、握っている間にゆっくり濃くなる。
    if (Has(state, Kind::SwordTip)) {
        float held = 0.0f;
        const float tipLocal = CueLocal(state, Kind::SwordTip, duration);
        if (tipLocal < 0.0f && CueLocal(state, Kind::SwordGrab, held) < 0.0f)
            return;
        const float x = tipLocal >= 0.0f ? std::clamp(tipLocal / duration, 0.0f, 1.0f) : 0.0f;
        if (state.tipSmokeHandle.IsValid()) {
            // 刀の根元の細かい域がある：そこへ、割れ目の線から細い煙を滲ませる。舞台の域（格が粗い）へは入れない。
            if (tipLocal >= 0.0f) {
                SeepFine(feed, intro, x);
                // そよ風：滲み出した煙が、立ちのぼりながら斜めに流れて、糸を引く。
                TipWind(feed, intro, kTipWindSeep * EaseOut(x), 0.0f, kTipWindSeepBack,
                        FluidChannels::kIntroSmoke | FluidChannels::kIntroTip);
            }
            return;
        }
        // 握り始めから、割れ目の上の空気を温めておく（見えない。寄りに切り替わった所から、煙がすぐ立ちのぼる）。
        SlitWarm(feed, intro);
        if (tipLocal >= 0.0f) {
            // 出始めが速く、ゆるやかに濃くなる。刀が抜けて行く終わりの所で、もう一段濃くなる。
            SlitSmoke(feed, intro,
                      intro.settings.seepAmount * (kTipSlitGain * EaseOut(x) + kTipSlitSurge * x * x * x * x));
            TipWind(feed, intro, kTipWindSeep * EaseOut(x), 0.0f, kTipWindSeepBack);
        }
        return;
    }
    const float local = CueLocal(state, Kind::SwordGrab, duration);
    if (local < 0.0f)
        return;
    const Vec3 entry = BossIntroActor::SwordEntry(intro);
    const float u = local / duration;
    const float mass = intro.settings.seepAmount * u / static_cast<float>(kSeepGround + kSeepBlade);
    for (int k = 0; k < kSeepGround; ++k) {
        const uint64_t id = kSeepId + static_cast<uint64_t>(k);
        const Vec3 out = Around(state.showSeconds * 1.7f + 2.0f * kPi * (static_cast<float>(k) + feed.Roll(id, 4u)) /
                                                               static_cast<float>(kSeepGround));
        Wisp wisp{};
        wisp.at = Add(entry, Add(Mul(out, 0.75f), Up(0.18f)));
        wisp.spread = 0.12f;
        wisp.radius = 0.22f;
        wisp.velocity = Add(Add(Mul(Tangent(out), kSeepSwirl), Mul(out, -kSeepInward)), Up(kSeepRise));
        wisp.gust = 0.8f;
        wisp.heat = kSeepHeat;
        feed.Smoke(id, wisp, mass);
    }
    for (int k = 0; k < kSeepBlade; ++k) {
        const uint64_t id = kSeepId + static_cast<uint64_t>(kSeepGround + k);
        Wisp wisp{};
        wisp.at = Add(entry, Up(0.3f + 1.3f * feed.Roll(id, 4u)));
        wisp.spread = 0.08f;
        wisp.radius = 0.2f;
        wisp.velocity = Up(kSeepRise);
        wisp.gust = 0.8f;
        wisp.heat = kSeepHeat + 0.6f;
        feed.Smoke(id, wisp, mass * u);
    }
}

// 握っている間（Sword.Grab）：外の地面から、黒い霧がゆっくり渦を巻いて根元へ寄り、そこから自分で立ち上がって行く。
// 点は外から内へ螺旋に走るので、地面に帯を引く。周りの空気も弱く根元へ寄せる。握るほど濃くなる。
void Gather(const Feed& feed, const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    float duration = 0.0f;
    // 剣先の寄りの段がある演出では、その段の間だけ、空気を根元へ寄せて持ち上げる（割れ目から滲み出した煙が、刀身へ
    // 巻き付いて上る）。外から寄る霧の点は出さない：止まった点の煙は球の塊に見え、寄りのカメラの前も横切る。
    const bool tip = Has(state, Kind::SwordTip);
    const float local = CueLocal(state, tip ? Kind::SwordTip : Kind::SwordGrab, duration);
    if (local < 0.0f)
        return;
    const Vec3 entry = BossIntroActor::SwordEntry(intro);
    const float u = tip ? EaseOut(local / duration) : local / duration;
    // 刀の根元の細かい域がある時は、そこへも届かせる。
    const uint32_t channels = FluidChannels::kIntroSmoke | (tip ? FluidChannels::kIntroTip : 0u);
    for (int k = 0; k < (tip ? 0 : kGatherWisps); ++k) {
        const float run = static_cast<float>(k) / static_cast<float>(kGatherWisps) + state.showSeconds * kGatherSpeed;
        const float phase = run - std::floor(run);
        const Vec3 out = Around(kPi * 0.5f * static_cast<float>(k) + phase * 2.2f + state.showSeconds * 0.3f);
        Wisp wisp{};
        wisp.at = Add(entry, Add(Mul(out, Lerp(kGatherFrom, 1.0f, phase)), Up(0.28f)));
        wisp.radius = 0.34f;
        wisp.velocity = Add(Add(Mul(out, -kGatherInward), Mul(Tangent(out), kGatherSwirl)), Up(kSeepRise));
        wisp.gust = 0.8f;
        wisp.heat = kSeepHeat;
        feed.Smoke(kGatherId + static_cast<uint64_t>(k), wisp,
                   intro.settings.gatherAmount * u / static_cast<float>(kGatherWisps));
    }
    for (int k = 0; k < kGatherPulls; ++k) {
        const Vec3 out =
            Around(state.showSeconds * 0.5f + 2.0f * kPi * static_cast<float>(k) / static_cast<float>(kGatherPulls));
        feed.Blow(kGatherId + 4 + static_cast<uint64_t>(k), Add(entry, Add(Mul(out, 2.4f), Up(0.5f))), 2.6f,
                  Normalize(Add(Mul(out, -1.0f), Mul(Tangent(out), 0.5f))), kGatherPull * u, channels);
    }
    // 根元に寄った霧を、ゆっくり持ち上げる（噴き上げない。温かさと弱い力で、自分から立ち上がって見える）。剣先の寄りの
    // 段がある演出では、根元の低い所で（割れ目から滲み出した薄い煙を、刀身に沿って上らせる）。
    feed.Blow(kGatherId + 7, Add(entry, Up(tip ? kTipLiftHeight : 1.6f)), tip ? kTipLiftRadius : 1.8f, Up(1.0f),
              (tip ? kTipLift : kGatherLift) * u, channels);
}

// 見得の黒い霧（抜刀の後から演技の終わりまで）：主役の両脇から後ろへ回り込みながら立ち上がる壁（点は弧に沿って走り、
// 力がゆっくりした渦と上昇をつくる）と、両脇の地面を前へ這い出す霧（こちらへ迫って来る）。主役の前へは入れない
// （体の周りの薄い煙は、別に消し続けている）。
void Backdrop(const Feed& feed, const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    const float since = Since(state, Kind::SwordOut);
    // 霧が晴れ始めたら、もう足さない。
    if (Since(state, Kind::SmokeDisperse) >= 0.0f)
        return;
    if (since < kBackdropAt || intro.settings.backdropAmount <= 0.0f)
        return;
    const float ramp = Smooth((since - kBackdropAt) / kBackdropRise);
    // 剣先の寄りの段がある演出：抜刀の段が終わった後も、弱い風を残す（霧が一つ所に立ち続けず、流れて行く。段の間は
    // DrawSpill が吹かせている）。
    const bool tip = Has(state, Kind::SwordTip);
    float drawSeconds = 0.0f;
    if (tip && CueLocal(state, Kind::SwordOut, drawSeconds) < 0.0f)
        TipWind(feed, intro, kTipWindGustLow * kTipWindBreeze, kTipWindGustWide * kTipWindBreeze, kTipWindGustBack);
    // 同じ演出：出始めの間は、点を地面の下へ沈めておく（霧が、主役の周りの地面から幕になって立ちのぼる。空中の点から
    // 出すと、出始めが黒い球に見える）。霧が立ってから、元の高さへ上げる。sunk＝沈めている割合。
    const float sunk = tip ? 1.0f - Smooth((since - kBackdropAt - kTipWallSunkHold) / kTipWallSunkRise) : 0.0f;
    const Vec3 feet = BossIntroSmoke::FeetWorld(intro);
    const Vec3 forward = Forward(state.yaw);
    const Vec3 left = Tangent(forward);
    if (sunk > 0.0f) {
        // 霧の立つ所（両脇と後ろ）の空気を温める：沈めた点からは温度がほとんど入らない。
        const Vec3 around[3] = {Mul(left, kTipWallWarmAway), Mul(left, -kTipWallWarmAway),
                                Mul(forward, -kTipWallWarmAway)};
        for (uint64_t k = 0; k < 3u; ++k)
            feed.Warm(kBackdropId + 15 + k, Add(feet, Add(around[k], Up(kTipWallWarmHeight))), kTipWallWarmRadius,
                      kTipWallWarmHeat * sunk);
    }
    const float wall = intro.settings.backdropAmount * 0.7f * ramp / static_cast<float>(kBackdropWisps);
    for (int k = 0; k < kBackdropWisps; ++k) {
        const uint64_t id = kBackdropId + static_cast<uint64_t>(k);
        const float side = k % 2 == 0 ? 1.0f : -1.0f;
        const float run =
            static_cast<float>(k) / static_cast<float>(kBackdropWisps) + state.showSeconds * kBackdropSpeed;
        const float phase = run - std::floor(run);
        // 主役の真横（70 度）から真後ろの近く（170 度）へ。弧そのものも、ゆっくり前後へずれる（壁を一つ所に留めない）。
        const float wander =
            kBackdropWander * std::sin(state.showSeconds * kBackdropWanderSpeed + 1.3f * static_cast<float>(k));
        const float angle = (Lerp(70.0f, 170.0f, phase) + wander) * kRadiansPerDegree;
        const Vec3 out = Add(Mul(forward, std::cos(angle)), Mul(left, side * std::sin(angle)));
        const Vec3 along = Add(Mul(forward, -std::sin(angle)), Mul(left, side * std::cos(angle)));
        Wisp wisp{};
        wisp.at = Add(feet, Add(Mul(out, kBackdropRadius + 0.9f * feed.Roll(id, 4u)),
                                Up(Lerp(0.4f + 4.8f * feed.Roll(id, 5u), -kTipSlitDepth, sunk))));
        wisp.spread = 0.5f * (1.0f - sunk);
        wisp.radius = Lerp(0.55f, kWispRadiusMin, sunk);
        wisp.velocity = Add(Mul(along, 2.0f), Up(1.2f));
        wisp.gust = 1.5f;
        wisp.heat = 0.9f;
        feed.Smoke(id, wisp, wall * Lerp(1.0f, kTipWallSunkGain, sunk));
    }
    const float drift = state.showSeconds * kBackdropWanderSpeed;
    for (int k = 0; k < 2; ++k) {
        const float side = k == 0 ? 1.0f : -1.0f;
        const uint64_t id = kBackdropId + 8 + static_cast<uint64_t>(k);
        // 力の向きと強さを左右で別々に揺らす（霧が同じ形のまま立ち続けない）。
        const float sway = std::sin(drift * 1.4f + side * 1.7f);
        // 両脇の地面を前へ這い出す霧。
        Wisp wisp{};
        wisp.at = Add(feet, Add(Add(Mul(left, side * 2.4f), Mul(forward, -0.6f + 1.2f * feed.Roll(id, 4u))),
                                Up(Lerp(0.25f, -kTipSlitDepth, sunk))));
        wisp.spread = 0.25f * (1.0f - sunk);
        wisp.radius = 0.38f;
        wisp.velocity = Mul(forward, 3.0f);
        wisp.gust = 1.0f;
        feed.Smoke(id, wisp, intro.settings.backdropAmount * 0.15f * ramp * Lerp(1.0f, kTipWallSunkGain, sunk));
        // 脇の霧を後ろへ巻き上げる流れと、地面の霧を前へ押し出す流れ。
        feed.Blow(kBackdropId + 10 + static_cast<uint64_t>(k), Add(feet, Add(Mul(left, side * 3.0f), Up(2.6f))), 3.2f,
                  Normalize(Add(Add(Mul(forward, -1.0f), Mul(left, side * 0.4f * sway)), Up(0.35f + 0.3f * sway))),
                  kBackdropSwirl * ramp, FluidChannels::kIntroSmoke);
        feed.Blow(kBackdropId + 12 + static_cast<uint64_t>(k), Add(feet, Add(Mul(left, side * 2.6f), Up(0.4f))), 2.4f,
                  forward, kBackdropCreep * ramp * (0.6f + 0.4f * sway), FluidChannels::kIntroSmoke);
    }
    // 後ろの壁を持ち上げ、主役の上へ少し覆いかぶさらせる。
    feed.Blow(kBackdropId + 14, Add(feet, Add(Mul(forward, -3.2f), Up(3.0f))), 3.5f,
              Normalize(Add(Add(Up(1.0f), Mul(forward, 0.2f)), Mul(left, 0.35f * std::sin(drift)))),
              kBackdropLift * ramp, FluidChannels::kIntroSmoke);
}

// 抜けた瞬間に根元から放射に走る煙の筋：点そのものを、根元から上と外へ速く飛ばす（60 m/秒より下）。エンジンが点の
// 動いた跡へ掃いて煙を入れるので、弧を描く帯になる（止まった点から入れた煙は、巻く前の丸い粒に見える。スローの寄りで
// 目立つ）。飛び出しの初めは域が光り始める温度より熱く、青白く光ってから黒い煙になる。outside＝刀のある側の外。
void DrawStreamers(const Feed& feed, const BossIntroSettings& s, Vec3 root, Vec3 outside, Vec3 forward, float local) {
    for (int k = 0; k < kBurstStreamers; ++k) {
        const float age = local - kBurstStreamerGap * static_cast<float>(k);
        if (age < 0.0f || age >= kBurstStreamerLife)
            continue;
        const uint64_t id = kStreamerId + static_cast<uint64_t>(k);
        const float u = age / kBurstStreamerLife;
        // 真上から、刀のある側の外へ扇に開く。奥行きは後ろへだけ（正面のカメラと主役の間を横切らせない）。
        const float turn =
            (kBurstStreamerFan * static_cast<float>(k) / static_cast<float>(kBurstStreamers - 1) - 8.0f) *
            kRadiansPerDegree;
        const float depth = (k % 2 == 0 ? -0.15f : -0.45f);
        const Vec3 aim = Normalize(Add(Add(Up(std::cos(turn)), Mul(outside, std::sin(turn))), Mul(forward, depth)));
        // 飛び出して減速する（速さは初めが最大）。
        const float travel = kBurstStreamerSpeed * kBurstStreamerLife * (u - 0.5f * u * u);
        Wisp wisp{};
        wisp.at = Add(root, Add(Up(0.3f), Mul(aim, travel)));
        wisp.radius = 0.4f;
        wisp.velocity = Mul(aim, kBurstStreamerSpeed * 0.2f);
        wisp.gust = 2.0f;
        wisp.heat = kBurstCoreHeat * (1.0f - u) * (1.0f - u) + 1.5f;
        feed.Smoke(id, wisp, s.drawBurstAmount * kBurstStreamerShare * (1.0f - 0.6f * u));
    }
}

// 主役から外へ、黒い霧を流す力（周りの 6 か所から、外へ少し上へ）。strength は 0〜1。
void DisperseFlow(const Feed& feed, const BossIntroComponent& intro, float strength) {
    if (strength <= 0.0f)
        return;
    const Vec3 feet = BossIntroSmoke::FeetWorld(intro);
    for (int k = 0; k < kDisperseBlows; ++k) {
        const Vec3 out = Around(2.0f * kPi * (static_cast<float>(k) + 0.5f) / static_cast<float>(kDisperseBlows));
        feed.Blow(kDisperseId + static_cast<uint64_t>(k), Add(feet, Add(Mul(out, 2.6f), Up(2.2f))), 4.2f,
                  Normalize(Add(out, Up(0.25f))), intro.settings.disperseBlow * strength, FluidChannels::kIntroSmoke);
    }
}

// 見得の終わり（拍 Smoke.Disperse）：黒い霧が主役から外へ流れて、薄れて行く。音も衝撃も付けない。流れはゆっくり立ち
// 上げ（いきなり吹き飛ばさない）、押し出した先で薄める。演技の後は、BossIntroSmoke::Render が同じ流れのまま消し切る。
void Disperse(const Feed& feed, BossIntroComponent& intro) {
    BossIntroState& state = intro.state;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::SmokeDisperse, duration);
    if (local < 0.0f)
        return;
    const float u = local / duration;
    const Vec3 feet = BossIntroSmoke::FeetWorld(intro);
    if (state.dispersePush) {
        state.dispersePush = false;
        feed.Burst(kDisperseId + 6, Add(feet, Up(2.5f)), kDisperseRadius, kDisperseBurst, FluidChannels::kIntroSmoke);
        feed.Burst(kDisperseId + 7, feet, kDisperseRadius * 1.5f, intro.settings.disperseFogPush, FluidChannels::kFog);
    }
    DisperseFlow(feed, intro, Smooth(u * 2.5f));
    // 押し出した先（周り）で薄める。後半は真ん中も。
    const float fade = kDisperseFade * Smooth(u * 1.5f);
    for (int k = 0; k < 4; ++k) {
        const Vec3 out = Around(0.5f * kPi * static_cast<float>(k));
        feed.Absorb(kDisperseId + 8 + static_cast<uint64_t>(k), Add(feet, Add(Mul(out, 6.5f), Up(3.0f))), 6.0f, fade);
    }
    feed.Absorb(kDisperseId + 12, Add(feet, Up(3.5f)), 7.0f, fade * Smooth(u * 2.0f - 0.6f));
}
