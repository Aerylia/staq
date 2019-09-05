/*
 * This file is part of synthewareQ.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "ast/traversal.h"
#include "mapping/device.h"

#include <map>

namespace synthewareQ {
namespace mapping {

  /**
   * \brief An initial layout based on the distribution of connections in the circuit
   *
   * Chooses a layout where the most coupled virtual qubits are assigned to the highest
   * fidelity couplings. Should perform well for devices with a high degree of connectivity
   */
  class BestFit final : public ast::PostVisitor {
  public:
    BestFit(Device& device) : PostVisitor(), device_(device) {}
    ~BestFit() = default;

    layout generate(ast::Program& prog) {
      allocated_ = std::vector<bool>(device_.qubits_, false);
      access_paths_.clear();
      histogram_.clear();  

      prog.accept(*this);

      return fit_histogram();
    }

    // Ignore gate declarations
    void visit(ast::GateDecl&) override { }

    void visit(ast::RegisterDecl& decl) override {
      if (decl.is_quantum()) {
        for (int i = 0; i < decl.size(); i++)
          access_paths_.insert(ast::VarAccess(decl.pos(), decl.id(), i));
      }
    }

    void visit(ast::CNOTGate& gate) override {
      histogram_[std::make_pair(gate.ctrl(), gate.tgt())] += 1;
    }

  private:
    Device device_;
    std::vector<bool> allocated_;
    std::set<ast::VarAccess> access_paths_;
    std::map<std::pair<ast::VarAccess, ast::VarAccess>, int> histogram_;

    layout fit_histogram() {
      layout ret;

      // Sort in order of decreasing number of two-qubit gates
      using mapping = std::pair<std::pair<ast::VarAccess, ast::VarAccess>, int>;
      using comparator = std::function<bool(mapping, mapping)>;
      comparator cmp = [](mapping a, mapping b) { return a.second > b.second; };
      std::set<mapping,comparator> sorted_pairs(histogram_.begin(), histogram_.end(), cmp);

      // For each pair with CNOT gates between them, try to assign a coupling
      auto couplings = device_.couplings();
      for (auto& [args, val] : sorted_pairs) {
        int ctrl_bit;
        int tgt_bit;
        for (auto& [coupling, f] : couplings) {
          if (auto it = ret.find(args.first); it != ret.end()) {
            if (it->second != coupling.first) continue;
            else ctrl_bit = it->second;
          } else if (!allocated_[coupling.first]) {
            ctrl_bit = coupling.first;
          } else {
            continue;
          }

          if (auto it = ret.find(args.second); it != ret.end()) {
            if (it->second != coupling.second) continue;
            else tgt_bit = it->second;
          } else if (!allocated_[coupling.second]) {
            tgt_bit = coupling.second;
          } else {
            continue;
          }

          ret[args.first] = ctrl_bit;
          ret[args.second] = tgt_bit;
          allocated_[ctrl_bit] = true;
          allocated_[tgt_bit] = true;
          couplings.erase(std::make_pair(coupling, f));
          break;
        }
      }

      // For any remaining access paths, map them
      for (auto ap : access_paths_) {
        auto i = 0;
        bool cont = ret.find(ap) == ret.end();
        while (cont) {
          if (i >= device_.qubits_) {
            std::cerr << "Error: can't fit program onto device " << device_.name_ << "\n";
            return ret;
          } else if (!allocated_[i]) {
              ret[ap] = i;
              allocated_[i] = true;
              cont = false;
          }

          i++;
        }
      }

      return ret;
    }
  };

  layout compute_bestfit_layout(Device& device, ast::Program& prog) {
    BestFit gen(device);
    return gen.generate(prog);
  }

}
}
