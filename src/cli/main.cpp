#include "core/numerics.hpp"
#include "core/verification.hpp"
#include "json.hpp"
#include "platform/atomic_file.hpp"
#include <sstream>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

int main(int argc,char** argv) {
    try {
        if(argc!=4 || std::string(argv[1])!="--run-all-verifications" || std::string(argv[2])!="--out") {
            std::cerr<<"Usage: wb_cli --run-all-verifications --out DIRECTORY\n"; return 2;
        }
        const std::filesystem::path dir(argv[3]); std::filesystem::create_directories(dir);
        nlohmann::json report={{"scope","implemented core module checks; not full specification acceptance"}};
        std::ostringstream fea; fea<<std::setprecision(17)<<"elements,h_m,nodal_error_C,midpoint_error_C\n";
        bool passed=true;
        for(int n:{10,20,40,80}) {
            wb::Rod r; r.elements=n; const auto solution=wb::solve(r);
            const double target=125.0/(n*n);
            passed=passed && std::abs(solution.midpoint_error-target)<1e-8;
            fea<<n<<','<<1.0/n<<','<<solution.nodal_error<<','<<solution.midpoint_error<<'\n';
            report["fea"].push_back({{"elements",n},{"midpoint_error_C",solution.midpoint_error},{"nodal_error_C",solution.nodal_error}});
        }
        wb::Parameters p{0.5,0,1,0}; const auto initial=wb::forward(p);
        const auto errors=wb::gradient_errors(p);
        for(double e:errors) passed=passed && e<=1e-5;
        report["backprop"]={{"initial_loss",initial.loss},{"gradient",initial.gradient},{"finite_difference_errors",errors},{"first_update",wb::update(p)}};
        std::ostringstream loss; loss<<std::setprecision(17)<<"step,loss\n";
        for(int i=0;i<=101;++i) { loss<<i<<','<<wb::forward(p).loss<<'\n'; if(i<101) p=wb::update(p); }
        report["backprop"]["loss_after_101_updates"]=wb::forward(p).loss;
        report["numerical_checks_passed"]=passed;
        for(const auto& check:wb::verify_modules()) {
            report["module_checks"].push_back({{"name",check.name},{"passed",check.passed},{"measured",check.measured},{"units",check.units}});
            passed=passed && check.passed;
        }
        report["implemented_checks_passed"]=passed;
        report["full_specification_complete"]=false;
        report["pending"]={"browser/WebGL acceptance","current Windows launch","complete evidence manifest","submission PDF","human handwritten appendix"};
        if(!fea||!loss) throw std::runtime_error("Unable to write numerical CSV files");
        std::atomic<bool> cancelled{false};
        wb::atomic_write(dir/"fea.csv",fea.str(),cancelled);
        wb::atomic_write(dir/"loss.csv",loss.str(),cancelled);
        wb::atomic_write(dir/"numerics.json",report.dump(2)+"\n",cancelled);
        std::cout<<report.dump(2)<<'\n'; return passed?0:1;
    } catch(const std::exception&) { std::cerr<<"Verification failed; check parameters and output folder permissions.\n"; return 1; }
}
