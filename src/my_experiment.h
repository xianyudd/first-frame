#ifndef L4_MY_EXPERIMENT_H
#define L4_MY_EXPERIMENT_H
struct ExperimentNotes {
    bool recorded;
    const char* change;
    const char* prediction;
    const char* observation;
    const char* explanation;
};
// TODO(L4-06): 只改一个数值做实验，记录改动、预测、观察、解释，交给老师人工验收。
inline ExperimentNotes myExperiment = {false, "", "", "", ""};
#endif
