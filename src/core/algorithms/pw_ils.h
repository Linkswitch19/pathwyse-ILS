#ifndef PW_ILS_H
#define PW_ILS_H

#include "algorithm.h"
#include "data/path.h"
#include "data/problem.h"

#include <chrono>
#include <random>

class PW_ILS : public Algorithm {
public:
    /**
     * @brief Construct a new PW_ILS object
     * 
     * @param name Algorithm identifier name
     * @param problem Pointer to the problem instance
     */
    PW_ILS(std::string name, Problem* problem);

    /**
     * @brief Default destructor
     * 
     */
    virtual ~PW_ILS() = default;

    /**
     * @brief Main method to run the Iterated Local Search algorithm.
     * Orchestrates the constructive phase, local search, and perturbation loop.
     */
    void solve() override;

    /**
     * @brief Resets the algorithm's state for a new run.
     * 
     * @param reset_level The level of reset to perform.
     */
    void resetAlgorithm(int reset_level) override;

private:
    // --- ILS Main Phases ---

    /**
     * @brief Creates an initial feasible solution using a cheapest insertion heuristic.
     * 
     * @return Path The generated initial solution.
     */
    Path constructivePhase();

    /**
     * @brief Improves a given solution by exploring its neighborhood.
     * Implements Add, Delete, and 2-opt operators with a Best Improvement strategy.
     * 
     * @param current_solution The solution to improve (passed by reference).
     */
    void localSearch(Path& current_solution);

    /**
     * @brief Perturbs a solution by randomly removing a percentage of its nodes.
     * 
     * @param solution The solution to perturb (passed by reference).
     */
    void perturb(Path& solution);

    // --- ILS Parameters ---

    double t_localsearch_limit;      // t: time limit in seconds for each local search phase
    int k_localsearch_iterations;    // k: max iterations for local search without improvement
    double p_perturb_percentage;     // p: percentage of nodes to remove during perturbation
    double td_ils_limit;             // td: total time limit in seconds for the entire ILS algorithm
    int kd_ils_iterations;           // kd: max iterations for the main ILS loop

    // --- Utilities ---

    std::mt19937 random_generator;   // Mersenne Twister random number generator
};

#endif // PW_ILS_H







