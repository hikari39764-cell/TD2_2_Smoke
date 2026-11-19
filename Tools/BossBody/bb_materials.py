"""ボスの外形の材質（テクスチャなしの PBR。色・金属・粗さだけで、漆・金・鋼・絹糸・革を見分けさせる）。"""
import bb_common as bb


def make():
    return {
        # 黒漆の鉄板：つやのある黒。真っ黒にすると夕方の逆光で黒い塊になるので、少しだけ明るく、空を映すようにしてある。
        "lacquer": bb.material("Lacquer", (0.030, 0.032, 0.040), metallic=0.35, roughness=0.30),
        # 金具（鍍金）
        "gold": bb.material("Gold", (0.83, 0.56, 0.20), metallic=1.0, roughness=0.36),
        # 鉄地（鍔・縁頭）
        "iron": bb.material("Iron", (0.07, 0.065, 0.06), metallic=1.0, roughness=0.55),
        # 縅の絹糸（深い紅）
        "silk": bb.material("Silk", (0.36, 0.022, 0.030), metallic=0.0, roughness=0.85),
        # 革・布（籠手の家地、手袋）
        "leather": bb.material("Leather", (0.022, 0.02, 0.022), metallic=0.0, roughness=0.8),
        # 鎖（籠手の鎖帷子の所）
        "mail": bb.material("Mail", (0.16, 0.16, 0.17), metallic=1.0, roughness=0.5),
        # 刀身：地（暗めの鏡）と刃（白く霞んだ焼き刃）。太陽を映して白く飛ばないよう、粗さを上げてある。
        "blade": bb.material("BladeSteel", (0.14, 0.15, 0.17), metallic=1.0, roughness=0.38),
        "blade_edge": bb.material("BladeEdge", (0.46, 0.48, 0.52), metallic=1.0, roughness=0.55),
        # 灼けた刀身（火の技）：地は暗い橙、刃は白に近い黄。自分で光る。
        "blade_hot": bb.material("BladeHot", (0.25, 0.04, 0.01), metallic=0.6, roughness=0.5,
                                 emission=(1.0, 0.22, 0.03), emission_strength=8000.0),
        "blade_hot_edge": bb.material("BladeHotEdge", (0.9, 0.6, 0.3), metallic=0.3, roughness=0.5,
                                      emission=(1.0, 0.72, 0.30), emission_strength=30000.0),
        # 灼けた鎧（フェーズ 2。c-857ae3）：草摺の板をつなぐ紅の糸が熾火に光る。
        "ember_silk": bb.material("EmberSilk", (0.30, 0.03, 0.01), metallic=0.0, roughness=0.8,
                                  emission=(1.0, 0.22, 0.02), emission_strength=1200.0),
        # 鮫皮と柄巻
        "same": bb.material("Same", (0.72, 0.69, 0.60), metallic=0.0, roughness=0.9),
        "ito": bb.material("Ito", (0.02, 0.022, 0.03), metallic=0.0, roughness=0.75),
    }
