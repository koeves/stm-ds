#ifndef TX_SKIP_HPP
#define TX_SKIP_HPP

#include "SkiplistNode.hpp"
#include "../STM/EncounterModeTx.hpp"
#include "../STM/CommitModeTx.hpp"
#include "../STM/Transaction.hpp"
#include "../STM/Orec.hpp"
#include "../Utilities/Util.hpp"

#define ___1 EncounterModeTx<SkiplistNode<T>*>
#define ___2 CommitModeTx<SkiplistNode<T>*>

#define TX__ ___2

template<class T = int>
class TransactionalSkiplist {
public:

    TransactionalSkiplist() :
        head(new SkiplistNode<T>(-1, MAX_LEVEL)), 
        level(0)
    {}

    ~TransactionalSkiplist() { 
        SkiplistNode<T> *node = head->neighbours[0];
        while (node != NULL) {
            SkiplistNode<T> *old = node;
            node = node->neighbours[0];
            delete old;
        }
        delete head;
    }

    void add(T val) { add(new SkiplistNode<T>(val, get_random_height())); }

    void display() {
        for (int i = level; i >= 0; i--) {
            SkiplistNode<T> *node = head->neighbours[i];
            std::cout << "Level " << i << ": ";
            while (node != NULL) {
                std::cout << node->value << " ";
                node = node->neighbours[i];
            }
            std::cout << "\n";
        }
    }

    int nthreads, ninserts;

private:

    SkiplistNode<T> *head;
    int level;

    static constexpr double PROB = 0.5;
    static const int MAX_LEVEL = 6;

    double get_random_prob() {
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_real_distribution<double> dist(0., 1.);

        return dist(mt);
    }

    int get_random_height() {
        double r = get_random_prob();
        int level = 0;
        while (r < PROB && level < MAX_LEVEL) {
            level++;
            r = get_random_prob();
        }
        return level;
    }

    void add(SkiplistNode<T> *n) {
        using AbortException = typename TX__::AbortException;

        TX__ Tx;
        bool done = false;
        //int num_aborts = 0, num_runs = 0;
        //std::string filename = "skip_ctx_aborts_100000_"+std::to_string(nthreads)+".txt";
        //std::ofstream outfile(filename, std::ios_base::app);
        while (!done) {
            try {
                Tx.begin();
                //num_runs++;

                SkiplistNode<T> *curr = Tx.read(&head);
                SkiplistNode<T> *update[MAX_LEVEL + 1] = {0};

                for (int i = Tx.read(&level); i >= 0; i--) {
                    while (Tx.read(&curr->neighbours[i]) && 
                           Tx.read(&curr->neighbours[i]->value) < n->value) {
                        curr = Tx.read(&curr->neighbours[i]);
                    }
                    update[i] = curr;
                }

                if (!Tx.read(&curr->neighbours[0]) || 
                     Tx.read(&curr->neighbours[0]->value) != n->value) {
                    int h = n->height;

                    if (h > Tx.read(&level)) {
                        for (int i = Tx.read(&level) + 1; i < h + 1; i++)
                            update[i] = Tx.read(&head);

                        Tx.write(&level, h);
                    }

                    for (int i = 0; i <= h; i++) {
                        Tx.write(&n->neighbours[i], update[i]->neighbours[i]);
                        Tx.write(&update[i]->neighbours[i], n);
                    }
                }

                done = Tx.commit();
                
                //outfile << (double)num_aborts/num_runs << '\n';
            }
            catch (AbortException&) {
                Tx.abort();
                done = false;
                //num_aborts++;
            }
        }
    }

};

#endif
