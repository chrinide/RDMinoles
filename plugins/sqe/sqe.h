#ifndef SQE_H
#define SQE_H

namespace psi{ namespace sqe {

class ahat {

  private:

    bool is_normal_order();

  public:

    ahat();
    ~ahat();

    bool skip     = false;
    int sign      = 1;
    double factor = 1.0;

    std::vector<std::string> symbol;
    std::vector<bool> is_dagger;
    std::vector<std::string> delta1;
    std::vector<std::string> delta2;
    std::vector<std::string> tensor;

    std::string lhs;

    void print();
    void print_code(Options& options);
    void check_spin();
    void normal_order(std::vector<ahat *> &ordered);
    void alphabetize(std::vector<ahat *> &ordered);
    void cleanup(std::vector<ahat *> &ordered);

};

}}

#endif
