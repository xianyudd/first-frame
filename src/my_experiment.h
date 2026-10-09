#ifndef L4_MY_EXPERIMENT_H
#define L4_MY_EXPERIMENT_H

enum class ExperimentVariable { None, HealAmount, FireRateMultiplier };
struct ExperimentScenario {
    bool recorded = false;
    int hp = 0, maxHp = 0;
    float shotInterval = 0;
    // HealAmount 测 HP；FireRateMultiplier 测射击间隔（秒）。
    float predictedBaseline = 0, predictedExperiment = 0;
    float observedBaseline = 0, observedExperiment = 0;
    bool predictionMatched = false; // 两个预测都吻合才为 true。
    float actualChange = 0; // 实验观察 - 基线观察。
    float baselineError = 0, experimentError = 0; // 观察 - 预测（保留正负号）。
};
struct ExperimentSpec {
    bool submitted = false;
    ExperimentVariable variable = ExperimentVariable::None;
    int healAmount = 30;
    float fireRateMultiplier = 0.8f;
    ExperimentScenario normal{}, boundary{};
    const char* explanation = ""; // 供交流；自动评分不判断文字质量。
};
// TODO(L4-06): 只改变一个非默认参数，记录普通/边界初态、预测、实测与数值分析。
// 默认未提交；不要改生产默认值。参数仅在 make test 的实验作用域内生效。
inline ExperimentSpec myExperiment{};
#endif
