#ifndef SIM_BP_H
#define SIM_BP_H

#include <vector>
#include <string>
#include <stdint.h> // gradescope uint stuff
#include <cstdlib>  // gradescope exit() EXIT_FAILURE

using namespace std;

typedef struct bp_params{
    unsigned long int K;  // number of bits of PC used to index the chooser table
    unsigned long int M1; // number of lower PC bits used to form prediction table index (gshare)
    unsigned long int M2; // number of lower PC bits used to form prediction table index (bimodal)
    unsigned long int N;  // number of bits in the global history register (gshare)
    char*             bp_name;
}bp_params;

enum Predictor_Type{
    bimodal,
    gshare,
    hybrid
};

class Predictor{
    private:
        Predictor_Type pred;

        uint32_t bim_entries_num;

        uint32_t gshare_entries_num;
        uint32_t gbhr_shift_amnt;
        uint32_t gbhr_mask;  
        uint32_t gbhr;
        
        uint32_t chooser_mask;
        uint32_t chooser_entries_num;

        vector<uint8_t> bimodal_table;
        vector<uint8_t> gshare_and_gbhr_table; // BHR + gshare table combined
        vector<uint8_t> chooser_table; 

        void init_bimodal(uint32_t M2);
        void init_gshare(uint32_t M1, uint32_t N);
        void init_hybrid(uint32_t M2, uint32_t M1, uint32_t N, uint32_t K);

        bool bimodal_prediction(uint32_t PC, bool actual_direction);
        bool gshare_prediction(uint32_t PC, bool actual_direction);
        bool hybrid_prediction(uint32_t PC, bool bim_pred, bool gsh_pred);

        void update_bimodal_table(uint32_t PC, bool actual_direction);
        void update_gbhr(bool actual_direction);
        void update_gshare_table(uint32_t PC, bool actual_direction);
        void update_chooser_table(uint32_t PC, bool bim_pred, bool gsh_pred, bool actual_direction);


    public:
        uint32_t misprediction_cnt;
        uint32_t branch_inst_cnt;

        // Constructor for 3 types of predictors
        // bimodal, gshare, and hybrid/tournament 
        Predictor(Predictor_Type pred, uint32_t M2=0, uint32_t M1=0, uint32_t N=0, uint32_t K=0);

        // Destructor
        ~Predictor();

        void predict(uint32_t PC, bool taken);
        void print_table_entries();
};

void Predictor::init_bimodal(uint32_t M2){
    misprediction_cnt = 0;
    branch_inst_cnt   = 0;

    bim_entries_num   = 1 << M2; // 2^M2

    for(uint32_t i = 0; i < bim_entries_num; i++){
        bimodal_table.push_back(2);
    }
}

void Predictor::init_gshare(uint32_t M1, uint32_t N){
    misprediction_cnt   = 0;
    branch_inst_cnt     = 0;

    gbhr_shift_amnt     = M1-N;
    gshare_entries_num  = 1 << M1;  
    gbhr_mask           = N == 0 ? 0 : 1 << (N-1);
    gbhr                = 0;

    for(uint32_t i = 0; i < gshare_entries_num; i++){
        gshare_and_gbhr_table.push_back(2);
    }
}

void Predictor::init_hybrid(uint32_t M2, uint32_t M1, uint32_t N, uint32_t K){
    init_bimodal(M2);
    init_gshare(M1, N);

    chooser_mask        = ~(~0u << (K));

    chooser_entries_num = 1 << K;

    for(uint32_t i = 0; i < chooser_entries_num; i++){
        chooser_table.push_back(1);
    }
}

Predictor::Predictor(Predictor_Type pred, uint32_t M2, uint32_t M1, uint32_t N, uint32_t K){

    this->pred = pred;

    switch (pred){
        case bimodal:
            init_bimodal(M2);
            break;

        case gshare:
            init_gshare(M2, M1); // M1 and N
            break;

        case hybrid:
            init_hybrid(M2, M1, N, K);
            break;
        
        default:
            printf("Error: Wrong predictor");
            exit(EXIT_FAILURE);
            break;
    }

}

void Predictor::update_bimodal_table(uint32_t PC, bool actual_direction){
    uint32_t idx;

    PC >>= 2; // Discard the last 2 bits of PC because they're always 0
    idx = PC % bim_entries_num;

    if(actual_direction){
        if(bimodal_table[idx] < 3){
            bimodal_table[idx]++;
        }
    }else{
        if(bimodal_table[idx] > 0){
            bimodal_table[idx]--;
        }
    }
}

bool Predictor::bimodal_prediction(uint32_t PC, bool actual_direction){
    uint32_t idx; 

    PC >>= 2; // Discard the last 2 bits of PC because they're always 0
    idx = PC % bim_entries_num;

    bool local_prediction = false;

    if (bimodal_table[idx] >= 2){
        local_prediction = true;
    }

    return local_prediction;
}

void Predictor::update_gbhr(bool actual_direction){
    if(actual_direction){
        gbhr = (gbhr >> 1) | gbhr_mask;
    }else{
        gbhr = (gbhr >> 1);
    }
}

void Predictor::update_gshare_table(uint32_t PC, bool actual_direction){
    uint32_t idx;

    PC >>= 2; 

    idx = PC % gshare_entries_num; 
    idx ^= (gbhr << (gbhr_shift_amnt));    // XOR n bits gbhr with only upper bits of PC

    if(actual_direction){
        if(gshare_and_gbhr_table[idx] < 3){
            gshare_and_gbhr_table[idx]++;
        }
    }else{
        if(gshare_and_gbhr_table[idx] > 0){
            gshare_and_gbhr_table[idx]--;
        }

    }
}

bool Predictor::gshare_prediction(uint32_t PC, bool actual_direction){
    uint32_t idx;

    PC >>= 2; 

    idx = PC % gshare_entries_num; 
    idx ^= (gbhr << (gbhr_shift_amnt));    // XOR n bits gbhr with only upper bits of PC
    
    bool local_prediction = false;

    if (gshare_and_gbhr_table[idx] >= 2){
        local_prediction = true;
    }

    return local_prediction;
}

void Predictor::update_chooser_table(uint32_t PC, bool bim_pred, bool gsh_pred, bool act_dir){

    uint32_t idx = (PC >> 2) & chooser_mask;

    if (bim_pred == gsh_pred){
        return;
    }
    if(gsh_pred == act_dir){
        if(chooser_table[idx] < 3){
            chooser_table[idx]++;
        }
    }else{
        if(chooser_table[idx] > 0){
            chooser_table[idx]--;
        }
    }
}

bool Predictor::hybrid_prediction(uint32_t PC, bool bim_pred, bool gsh_pred){
    
    uint32_t idx = (PC >> 2) & chooser_mask;

    return chooser_table[idx] >= 2 ? gsh_pred : bim_pred;
}

void Predictor::predict(uint32_t PC, bool actual_direction){
    branch_inst_cnt++;

    bool prediction;

    switch (pred){
        case bimodal:
            prediction = bimodal_prediction(PC, actual_direction);
            if(actual_direction != prediction){
                misprediction_cnt++;
            }
            update_bimodal_table(PC, actual_direction);
            break;

        case gshare:
            prediction = gshare_prediction(PC, actual_direction);
            if(actual_direction != prediction){
                misprediction_cnt++;
            } 
            update_gshare_table(PC, actual_direction);
            update_gbhr(actual_direction);
            break;

        case hybrid:
            bool bim_pred = bimodal_prediction(PC, actual_direction);
            bool gsh_pred = gshare_prediction(PC, actual_direction);

            prediction = hybrid_prediction(PC, bim_pred, gsh_pred);
            if(actual_direction != prediction){
                misprediction_cnt++;
            }
            uint32_t chooser_idx = (PC >> 2) & chooser_mask;

            chooser_table[chooser_idx] >= 2 ? update_gshare_table(PC, actual_direction) : update_bimodal_table(PC, actual_direction);
            update_gbhr(actual_direction);
            update_chooser_table(PC, bim_pred, gsh_pred, actual_direction);

            break;
    }
}

void Predictor::print_table_entries(){
    switch (pred){
        case bimodal:
            printf("FINAL BIMODAL CONTENTS\n");
            for(uint32_t i = 0; i < bim_entries_num; i++){
                printf("%-3d%d\n", i, bimodal_table[i]);
            }
            break;

        case gshare:
            printf("FINAL GSHARE CONTENTS\n");
            for(uint32_t i = 0; i < gshare_entries_num; i++){
                if(i >= 100){
                    printf("%-4d\t%d\n", i, gshare_and_gbhr_table[i]);
                }else{
                    printf("%-3d%d\n", i, gshare_and_gbhr_table[i]);
                }
            }
            break;

        case hybrid:
            printf("FINAL CHOOSER CONTENTS\n");
            for(uint32_t i = 0; i < chooser_entries_num; i++){
                if(i >= 100){
                    printf("%-4d\t%d\n", i, chooser_table[i]);
                }else{
                    printf("%-3d%d\n", i, chooser_table[i]);
                }
            }
            printf("FINAL GSHARE CONTENTS\n");
            for(uint32_t i = 0; i < gshare_entries_num; i++){
                if(i >= 100){
                    printf("%-4d\t%d\n", i, gshare_and_gbhr_table[i]);
                }else{
                    printf("%-3d%d\n", i, gshare_and_gbhr_table[i]);
                }
            }
            printf("FINAL BIMODAL CONTENTS\n");
            for(uint32_t i = 0; i < bim_entries_num; i++){
                printf("%-3d%d\n", i, bimodal_table[i]);
            }
            break;
    }
}


#endif // SIM_BP_H
