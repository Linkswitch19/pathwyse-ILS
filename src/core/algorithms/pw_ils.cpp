#include "algorithms/pw_ils.h"
#include "data/problem.h"
#include "data/path.h"
#include "utils/param.h"

#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <random>

// --- Helper Function ---

/**
 * @brief Calculates the objective value and resource consumptions for a given tour.
 * 
 * @param problem Pointer to the problem instance.
 * @param tour The list of nodes representing the tour.
 * @param objective Output parameter for the calculated objective value.
 * @param consumption Output parameter for the vector of resource consumptions.
 * @return true if the path is feasible regarding resource constraints.
 * @return false if the path is infeasible.
 */
bool calculate_path_metrics(Problem* problem, const std::list<int>& tour, int& objective, std::vector<int>& consumption) {
    objective = 0;
    consumption.assign(problem->getNumRes(), 0);

    int prev_node = -1;
    for (int node : tour) {
        if (prev_node != -1) {
            //  costo dell'arco all'obiettivo
            objective += problem->getObj()->getArcCost(prev_node, node);

            //  consumo dell'arco alla risorsa (LA PARTE CHE MANCAVA!)
            for (int i = 0; i < problem->getNumRes(); ++i) {
                consumption[i] += problem->getRes(i)->getArcCost(prev_node, node);
            }
        }

        //  costo del nodo all'obiettivo
        objective += problem->getObj()->getNodeCost(node);

        // Aggiungiamo il consumo del nodo alla risorsa
        for (int i = 0; i < problem->getNumRes(); ++i) {
            if (node != problem->getOrigin() && node != problem->getDestination()) {
                consumption[i] += problem->getRes(i)->getNodeCost(node);
            }
        }
        prev_node = node;
    }

    // The destination node's cost (prize) is not part of the objective function in ESPPRC
    objective -= problem->getObj()->getNodeCost(problem->getDestination());

    for (int i = 0; i < problem->getNumRes(); ++i) {
        if (consumption[i] > problem->getRes(i)->getUB()) {
            return false; // Infeasible
        }
    }
    return true; // Feasible
}


// --- Constructor ---

PW_ILS::PW_ILS(std::string name, Problem* problem) : Algorithm(name, problem) {
    // Initialize parameters from the Parameters singleton or use defaults from the SRS.
    // Using the "ILS leggera" configuration as a default example.
    t_localsearch_limit = Parameters::getIlsT();
    k_localsearch_iterations = Parameters::getIlsK();
    p_perturb_percentage = Parameters::getIlsP();
    td_ils_limit = Parameters::getIlsTd();
    kd_ils_iterations = Parameters::getIlsKd();
    // Initialize random number generator with a seed for reproducibility if provided.
    random_generator.seed(std::random_device()());

   
    
}

// --- Core Algorithm Methods ---

void PW_ILS::resetAlgorithm(int reset_level) {
    initAlgorithm();
    clearSolutions();
}

void PW_ILS::solve() {
    // 1. Avvia il timer di PathWyse prima di iniziare
    collector.startGlobalTime();

    auto start_time = std::chrono::steady_clock::now();
    setStatus(ALGO_OPTIMIZING);

    // Phase 1: Constructive Phase
    Path current_solution = constructivePhase();
    if (current_solution.getStatus() == PATH_INFEASIBLE || current_solution.getTourLength() <= 2) {
        // ATTENZIONE: Ferma il timer anche in caso di uscita anticipata
        collector.stopGlobalTime();
        setStatus(ALGO_DONE);
        return; // No feasible solution found
    }

    // Phase 2: Initial Local Search
    localSearch(current_solution);

    Path best_solution = current_solution;
    updateIncumbent(best_solution.getObjective());

    // Phase 3: ILS Loop (Perturbation + Local Search + Acceptance)
    for (iterations = 0; iterations < kd_ils_iterations; ++iterations) {
        // Check total time limit
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        if (elapsed.count() > td_ils_limit || isTimeLimitReached()) {
            setStatus(ALGO_TIMELIMIT);
            break;
        }

        // Perturbation (from the best solution found so far)
        Path perturbed_solution = best_solution;
        perturb(perturbed_solution);

        // Local Search on perturbed solution
        localSearch(perturbed_solution);

        // Acceptance Criterion (only accept better solutions)
        if (perturbed_solution.getObjective() < best_solution.getObjective()) {
            best_solution = perturbed_solution;
            updateIncumbent(best_solution.getObjective());
        }
    }

    addSolution(best_solution);

    // 2. Ferma il timer alla fine dell'algoritmo
    collector.stopGlobalTime();
    setStatus(ALGO_DONE);
}

// --- Private Helper Methods: ILS Phases ---

Path PW_ILS::constructivePhase() {
    Path path;
    std::list<int> tour = { problem->getOrigin(), problem->getDestination() };

    std::vector<int> candidate_nodes;
    for (int i = 0; i < problem->getNumNodes(); ++i) {
        if (i != problem->getOrigin() && i != problem->getDestination()) {
            candidate_nodes.push_back(i);
        }
    }

    while (true) {
      
        double best_delta = std::numeric_limits<double>::max();
        int best_node_to_insert = -1;
        std::list<int>::iterator best_pos_iterator;

        for (int node_to_insert : candidate_nodes) {
            int pos_idx = 1; 
            for (auto it = tour.begin(); std::next(it) != tour.end(); ++it, ++pos_idx) {
                int u = *it;
                int v = *std::next(it);

                double delta = problem->getObj()->getArcCost(u, node_to_insert) +
                    problem->getObj()->getArcCost(node_to_insert, v) -
                    problem->getObj()->getArcCost(u, v) +
                    problem->getObj()->getNodeCost(node_to_insert);

                
                if (delta < best_delta) {
                    std::list<int> temp_tour = tour;
                    auto temp_it = temp_tour.begin();
                    std::advance(temp_it, pos_idx); 
                    temp_tour.insert(temp_it, node_to_insert);

                    int temp_obj;
                    std::vector<int> temp_cons;

                   
                    if (calculate_path_metrics(problem, temp_tour, temp_obj, temp_cons)) {
                        best_delta = delta;
                        best_node_to_insert = node_to_insert;
                        best_pos_iterator = it;
                    }
                }
            }
        }

    
        if (best_node_to_insert != -1) {
            tour.insert(std::next(best_pos_iterator), best_node_to_insert);
            candidate_nodes.erase(
                std::remove(candidate_nodes.begin(), candidate_nodes.end(), best_node_to_insert),
                candidate_nodes.end()
            );
        }
        else {
            
            break;
        }
    }

    path.setTour(tour);

    int final_objective;
    std::vector<int> final_consumption;
    if (calculate_path_metrics(problem, tour, final_objective, final_consumption)) {
        path.setObjective(final_objective);
        path.setConsumption(final_consumption);
        path.setStatus(PATH_FEASIBLE);
    }
    else {
        path.setStatus(PATH_INFEASIBLE);
    }

    return path;
}

void PW_ILS::localSearch(Path& current_solution) {
    auto start_time = std::chrono::steady_clock::now();
    int iters_no_improvement = 0;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        if (elapsed.count() > t_localsearch_limit || iters_no_improvement >= k_localsearch_iterations) {
            break;
        }

        int initial_objective = current_solution.getObjective();

       
        std::list<int> current_tour = current_solution.getTour();

        // --- 1. Add Operator (Best Improvement) ---
        int best_add_objective = current_solution.getObjective();
        std::list<int> best_add_tour;

        std::vector<bool> in_tour(problem->getNumNodes(), false);
        for (int node : current_tour) in_tour[node] = true;

        for (int node_to_add = 0; node_to_add < problem->getNumNodes(); ++node_to_add) {
            if (in_tour[node_to_add]) continue;

            int pos_idx = 1;
            for (auto it = current_tour.begin(); std::next(it) != current_tour.end(); ++it, ++pos_idx) {
                std::list<int> new_tour = current_tour;
                auto new_it = new_tour.begin();
                std::advance(new_it, pos_idx);
                new_tour.insert(new_it, node_to_add);

                int new_obj; std::vector<int> new_cons;
                if (calculate_path_metrics(problem, new_tour, new_obj, new_cons) && new_obj < best_add_objective) {
                    best_add_objective = new_obj;
                    best_add_tour = new_tour;
                }
            }
        }
        if (!best_add_tour.empty()) {
            current_solution.setTour(best_add_tour);
            int obj; std::vector<int> cons;
            calculate_path_metrics(problem, best_add_tour, obj, cons);
            current_solution.setObjective(obj);
            current_solution.setConsumption(cons);
            current_tour = best_add_tour; 
        }

        // --- 2. Delete Operator (Best Improvement) ---
        int best_del_objective = current_solution.getObjective();
        std::list<int> best_del_tour;

        
        std::vector<int> tour_vec_del(current_tour.begin(), current_tour.end());

        if (tour_vec_del.size() > 2) {
            for (size_t i = 1; i < tour_vec_del.size() - 1; ++i) {
                std::vector<int> new_tour_vec = tour_vec_del;
                new_tour_vec.erase(new_tour_vec.begin() + i);
                std::list<int> new_tour(new_tour_vec.begin(), new_tour_vec.end());

                int new_obj; std::vector<int> new_cons;
                
                if (calculate_path_metrics(problem, new_tour, new_obj, new_cons) && new_obj < best_del_objective) {
                    best_del_objective = new_obj;
                    best_del_tour = new_tour;
                }
            }
        }
        if (!best_del_tour.empty()) {
            current_solution.setTour(best_del_tour);
            int obj; std::vector<int> cons;
            calculate_path_metrics(problem, best_del_tour, obj, cons);
            current_solution.setObjective(obj);
            current_solution.setConsumption(cons);
            current_tour = best_del_tour; // Aggiorniamo la copia locale
        }

        // --- 3. 2-opt Operator (Best Improvement) ---
        int best_2opt_objective = current_solution.getObjective();
        std::list<int> best_2opt_tour;

      
        std::vector<int> tour_vec_2opt(current_tour.begin(), current_tour.end());

        if (tour_vec_2opt.size() > 3) {
            for (size_t i = 1; i < tour_vec_2opt.size() - 2; ++i) {
                for (size_t j = i + 1; j < tour_vec_2opt.size() - 1; ++j) {
                    std::vector<int> new_tour_vec = tour_vec_2opt;
                    std::reverse(new_tour_vec.begin() + i, new_tour_vec.begin() + j + 1);
                    std::list<int> new_tour(new_tour_vec.begin(), new_tour_vec.end());

                    int new_obj; std::vector<int> new_cons;
                    if (calculate_path_metrics(problem, new_tour, new_obj, new_cons) && new_obj < best_2opt_objective) {
                        best_2opt_objective = new_obj;
                        best_2opt_tour = new_tour;
                    }
                }
            }
        }
        if (!best_2opt_tour.empty()) {
            current_solution.setTour(best_2opt_tour);
            current_solution.setObjective(best_2opt_objective);
            int obj; std::vector<int> cons;
            calculate_path_metrics(problem, best_2opt_tour, obj, cons);
            current_solution.setConsumption(cons);
        }

        if (current_solution.getObjective() < initial_objective) {
            iters_no_improvement = 0;
        }
        else {
            iters_no_improvement++;
            break; // No improvement in a full cycle, exit LS
        }
    }
}

void PW_ILS::perturb(Path& solution) {
    std::list<int> tour = solution.getTour();
    if (tour.size() <= 3) return; // Not enough nodes to perturb

    std::vector<int> removable_nodes;
    for (auto it = std::next(tour.begin()); std::next(it) != tour.end(); ++it) {
        removable_nodes.push_back(*it);
    }

    if (removable_nodes.empty()) return;

    int num_to_remove = std::max(1, static_cast<int>(removable_nodes.size() * p_perturb_percentage));

    std::shuffle(removable_nodes.begin(), removable_nodes.end(), random_generator);

    for (int i = 0; i < num_to_remove; ++i) {
        tour.remove(removable_nodes[i]);
    }

    solution.setTour(tour);

    // Recalculate metrics for the new perturbed path
    int final_objective;
    std::vector<int> final_consumption;
    if (calculate_path_metrics(problem, tour, final_objective, final_consumption)) {
        solution.setObjective(final_objective);
        solution.setConsumption(final_consumption);
        solution.setStatus(PATH_FEASIBLE);
    } else {
        // This should not happen if perturbation only removes nodes
        solution.setStatus(PATH_INFEASIBLE);
    }
}





