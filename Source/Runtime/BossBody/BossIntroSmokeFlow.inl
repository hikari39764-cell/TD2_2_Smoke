// 登場の煙（c-e0c500）のうち、流れをつくる 3 つ：握る間に根元へ集まる霧（Gather）、見得の黒い霧（Backdrop）、抜けた瞬間に
// 根元から走る煙の筋（DrawStreamers）、見得の終わりに霧を晴らす流れ（Disperse）。BossIntroSmoke.cpp の無名の名前空間の中へ、そのまま取り込む（1 ファイル 800 行
// 以内に収めるために分けた。Feed・Wisp・CueLocal・Since と形の決まりは、取り込む側が先に用意する）。

// 握っている間（Sword.Grab）：外の地面から、黒い霧がゆっくり渦を巻いて根元へ寄り、そこから自分で立ち上がって行く。
// 点は外から内へ螺旋に走るので、地面に帯を引く。周りの空気も弱く根元へ寄せる。握るほど濃くなる。
void Gather(const Feed& feed, const BossComponent& boss) {
    const BossIntroState& state = boss.introState;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::SwordGrab, duration);
    if (local < 0.0f)
        return;
    const Vec3 entry = BossIntroActor::SwordEntry(boss);
    const float u = local / duration;
    for (int k = 0; k < kGatherWisps; ++k) {
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
                   boss.intro.gatherAmount * u / static_cast<float>(kGatherWisps));
    }
    for (int k = 0; k < kGatherPulls; ++k) {
        const Vec3 out =
            Around(state.showSeconds * 0.5f + 2.0f * kPi * static_cast<float>(k) / static_cast<float>(kGatherPulls));
        feed.Blow(kGatherId + 4 + static_cast<uint64_t>(k), Add(entry, Add(Mul(out, 2.4f), Up(0.5f))), 2.6f,
                  Normalize(Add(Mul(out, -1.0f), Mul(Tangent(out), 0.5f))), kGatherPull * u,
                  FluidChannels::kIntroSmoke);
    }
    // 根元に寄った霧を、ゆっくり持ち上げる（噴き上げない。温かさと弱い力で、自分から立ち上がって見える）。
    feed.Blow(kGatherId + 7, Add(entry, Up(1.6f)), 1.8f, Up(1.0f), kGatherLift * u, FluidChannels::kIntroSmoke);
}

// 見得の黒い霧（抜刀の後から演技の終わりまで）：主役の両脇から後ろへ回り込みながら立ち上がる壁（点は弧に沿って走り、
// 力がゆっくりした渦と上昇をつくる）と、両脇の地面を前へ這い出す霧（こちらへ迫って来る）。主役の前へは入れない
// （体の周りの薄い煙は、別に消し続けている）。
void Backdrop(const Feed& feed, const BossComponent& boss) {
    const BossIntroState& state = boss.introState;
    const float since = Since(state, Kind::SwordOut);
    // 霧が晴れ始めたら、もう足さない。
    if (Since(state, Kind::SmokeDisperse) >= 0.0f)
        return;
    if (since < kBackdropAt || boss.intro.backdropAmount <= 0.0f)
        return;
    const float ramp = Smooth((since - kBackdropAt) / kBackdropRise);
    const Vec3 feet = BossIntroSmoke::FeetWorld(boss);
    const Vec3 forward = Forward(state.yaw);
    const Vec3 left = Tangent(forward);
    const float wall = boss.intro.backdropAmount * 0.7f * ramp / static_cast<float>(kBackdropWisps);
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
        wisp.at =
            Add(feet, Add(Mul(out, kBackdropRadius + 0.9f * feed.Roll(id, 4u)), Up(0.4f + 4.8f * feed.Roll(id, 5u))));
        wisp.spread = 0.5f;
        wisp.radius = 0.55f;
        wisp.velocity = Add(Mul(along, 2.0f), Up(1.2f));
        wisp.gust = 1.5f;
        wisp.heat = 0.9f;
        feed.Smoke(id, wisp, wall);
    }
    const float drift = state.showSeconds * kBackdropWanderSpeed;
    for (int k = 0; k < 2; ++k) {
        const float side = k == 0 ? 1.0f : -1.0f;
        const uint64_t id = kBackdropId + 8 + static_cast<uint64_t>(k);
        // 力の向きと強さを左右で別々に揺らす（霧が同じ形のまま立ち続けない）。
        const float sway = std::sin(drift * 1.4f + side * 1.7f);
        // 両脇の地面を前へ這い出す霧。
        Wisp wisp{};
        wisp.at =
            Add(feet, Add(Add(Mul(left, side * 2.4f), Mul(forward, -0.6f + 1.2f * feed.Roll(id, 4u))), Up(0.25f)));
        wisp.spread = 0.25f;
        wisp.radius = 0.38f;
        wisp.velocity = Mul(forward, 3.0f);
        wisp.gust = 1.0f;
        feed.Smoke(id, wisp, boss.intro.backdropAmount * 0.15f * ramp);
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
void DisperseFlow(const Feed& feed, const BossComponent& boss, float strength) {
    if (strength <= 0.0f)
        return;
    const Vec3 feet = BossIntroSmoke::FeetWorld(boss);
    for (int k = 0; k < kDisperseBlows; ++k) {
        const Vec3 out = Around(2.0f * kPi * (static_cast<float>(k) + 0.5f) / static_cast<float>(kDisperseBlows));
        feed.Blow(kDisperseId + static_cast<uint64_t>(k), Add(feet, Add(Mul(out, 2.6f), Up(2.2f))), 4.2f,
                  Normalize(Add(out, Up(0.25f))), boss.intro.disperseBlow * strength, FluidChannels::kIntroSmoke);
    }
}

// 見得の終わり（拍 Smoke.Disperse）：黒い霧が主役から外へ流れて、薄れて行く。音も衝撃も付けない。流れはゆっくり立ち
// 上げ（いきなり吹き飛ばさない）、押し出した先で薄める。演技の後は、BossIntroSmoke::Render が同じ流れのまま消し切る。
void Disperse(const Feed& feed, BossComponent& boss) {
    BossIntroState& state = boss.introState;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::SmokeDisperse, duration);
    if (local < 0.0f)
        return;
    const float u = local / duration;
    const Vec3 feet = BossIntroSmoke::FeetWorld(boss);
    if (state.dispersePush) {
        state.dispersePush = false;
        feed.Burst(kDisperseId + 6, Add(feet, Up(2.5f)), kDisperseRadius, kDisperseBurst, FluidChannels::kIntroSmoke);
        feed.Burst(kDisperseId + 7, feet, kDisperseRadius * 1.5f, boss.intro.disperseFogPush, FluidChannels::kFog);
    }
    DisperseFlow(feed, boss, Smooth(u * 2.5f));
    // 押し出した先（周り）で薄める。後半は真ん中も。
    const float fade = kDisperseFade * Smooth(u * 1.5f);
    for (int k = 0; k < 4; ++k) {
        const Vec3 out = Around(0.5f * kPi * static_cast<float>(k));
        feed.Absorb(kDisperseId + 8 + static_cast<uint64_t>(k), Add(feet, Add(Mul(out, 6.5f), Up(3.0f))), 6.0f, fade);
    }
    feed.Absorb(kDisperseId + 12, Add(feet, Up(3.5f)), 7.0f, fade * Smooth(u * 2.0f - 0.6f));
}
