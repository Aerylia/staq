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

#include "mapping/device.h"
#include "synthesis/linear_reversible.h"
#include "utils/angle.h"

#include <vector>
#include <list>
#include <variant>

namespace synthewareQ {
namespace synthesis {

  using namespace mapping;
  using phase_term = std::pair<std::vector<bool>, utils::Angle>;
  using cx_dihedral = std::variant<std::pair<int, int>, std::pair<utils::Angle, int> >;

  struct partition {
    std::optional<int> target;
    std::set<int>      remaining_indices;
    std::list<phase_term> terms;
  };

  void print_partition(partition& part) {
    std::cout << "{";
    if (part.target) std::cout << *(part.target);
    else std::cout << "_";
    std::cout << ", [";
    for (auto i : part.remaining_indices) std::cout << i << ",";
    std::cout << "], {";
    for (auto& [vec, angle] : part.terms) {
      std::cout << angle << "*(";
      for (auto i = 0; i < vec.size(); i++) std::cout << (vec[i] ? "1" : "0");
      std::cout << "), ";
    }
    std::cout << "}}\n";
  }

  void adjust_vectors(int ctrl, int tgt, std::list<partition>& stack) {
    for (auto& part : stack) {
      for (auto& [vec, angle] : part.terms) {
        vec[ctrl] = vec[ctrl] ^ vec[tgt];
      }
    }
  }

  int find_best_split(const std::list<phase_term>& terms, const std::set<int>& indices) {
    int max = -1;
    int max_i = -1;
    for (auto i : indices) {
      auto num_zeros = 0;
      auto num_ones = 0;

      for (auto& [vec, angle] : terms) {
        if (vec[i]) num_ones++;
        else num_zeros++;
      }

      if (max_i == -1 || num_zeros > max || num_ones > max) {
        max = num_zeros > num_ones ? num_zeros : num_ones;
        max_i = i;
      }
    }

    return max_i;
  }

  std::pair<std::list<phase_term>, std::list<phase_term> > split(std::list<phase_term>& terms, int i) {
    std::list<phase_term> zeros;
    std::list<phase_term> ones;

    while(!terms.empty()) {
      if (terms.front().first[i]) ones.splice(ones.end(), terms, terms.begin());
      else zeros.splice(zeros.end(), terms, terms.begin());
    }

    return std::make_pair(zeros, ones);
  }

  std::list<cx_dihedral> gray_synth(const std::list<phase_term>& f, linear_op<bool> A) {
    // Initialize
    std::list<cx_dihedral> ret;
    std::list<partition> stack;

    std::set<int> indices;
    for (auto i = 0; i < A.size(); i++) indices.insert(i);

    stack.push_front({std::nullopt, indices, f});

    while (!stack.empty()) {
      auto part = stack.front();
      stack.pop_front();

      // Debug
      /*
      std::cout << "Processing partition:\n  ";
      print_partition(part);
      */

      if (part.terms.size() == 0) continue;
      else if (part.terms.size() == 1 && part.target) {
        // This case allows us to shortcut a lot of partitions

        auto tgt = *(part.target);
        auto& [vec, angle] = part.terms.front();

        for (auto ctrl = 0; ctrl < vec.size(); ctrl++) {
          if (ctrl != tgt && vec[ctrl]) {
            ret.push_back(std::make_pair((int)ctrl, (int)tgt));

            // Adjust remaining vectors & output function
            adjust_vectors(ctrl, tgt, stack);
            for (auto i = 0; i < A.size(); i++) {
              A[i][ctrl] = A[i][ctrl] ^ A[i][tgt];
            }
          }
        }

        ret.push_back(std::make_pair(angle, tgt));
      } else if (!part.remaining_indices.empty()) {
        // Divide into the zeros and ones of some row
        auto i = find_best_split(part.terms, part.remaining_indices);
        auto [zeros, ones] = split(part.terms, i);

        // Remove i from the remaining indices
        part.remaining_indices.erase(i);

        // Add the new partitions on the stack
        if (part.target) {
          stack.push_front({part.target, part.remaining_indices, ones});
        } else {
          stack.push_front({i, part.remaining_indices, ones});
        }
        stack.push_front({part.target, part.remaining_indices, zeros});
      } else {
        throw std::logic_error("No indices left to pivot on, but multiple vectors remain!\n");
      }

    }

    // Synthesize the overall linear transformation
    auto linear_trans = gauss_jordan(A);
    for (auto gate : linear_trans) ret.push_back(gate);

    return ret;
  }

  std::list<cx_dihedral> gray_steiner(const std::list<phase_term>& f, linear_op<bool> A, Device& d) {
    // Initialize
    std::list<cx_dihedral> ret;
    std::list<partition> stack;

    std::set<int> indices;
    for (auto i = 0; i < A.size(); i++) indices.insert(i);

    stack.push_front({std::nullopt, indices, f});

    while (!stack.empty()) {
      auto part = stack.front();
      stack.pop_front();

      // Debug
      /*
      std::cout << "Processing partition:\n  ";
      print_partition(part);
      */

      if (part.terms.size() == 0) continue;
      else if (part.terms.size() == 1 && part.target) {
        // This case allows us to shortcut a lot of partitions

        auto tgt = *(part.target);
        auto& [vec, angle] = part.terms.front();

        std::list<int> terminals;
        for (auto ctrl = 0; ctrl < vec.size(); ctrl++) {
          if (ctrl != tgt && vec[ctrl]) terminals.push_back(ctrl);
        }

        auto s_tree = d.steiner(terminals, tgt);

        // Fill each steiner point with a one
        for (auto it = s_tree.rbegin(); it != s_tree.rend(); it++) {
          if (vec[it->second] == 0) {
            ret.push_back(std::make_pair((int)(it->second), (int)(it->first)));
            adjust_vectors(it->second, it->first, stack);
            for (auto i = 0; i < A.size(); i++) {
              A[i][it->second] = A[i][it->second] ^ A[i][it->first];
            }
          }
        }
        
        // Zero out each row except for the root
        for (auto it = s_tree.rbegin(); it != s_tree.rend(); it++) {
          ret.push_back(std::make_pair((int)(it->second), (int)(it->first)));
          adjust_vectors(it->second, it->first, stack);
          for (auto i = 0; i < A.size(); i++) {
              A[i][it->second] = A[i][it->second] ^ A[i][it->first];
          }
        }

        ret.push_back(std::make_pair(angle, tgt));
      } else if (!part.remaining_indices.empty()) {
        // Divide into the zeros and ones of some row
        auto i = find_best_split(part.terms, part.remaining_indices);
        auto [zeros, ones] = split(part.terms, i);

        // Remove i from the remaining indices
        part.remaining_indices.erase(i);

        // Add the new partitions on the stack
        if (part.target) {
          stack.push_front({part.target, part.remaining_indices, ones});
        } else {
          stack.push_front({i, part.remaining_indices, ones});
        }
        stack.push_front({part.target, part.remaining_indices, zeros});
      } else {
        throw std::logic_error("No indices left to pivot on, but multiple vectors remain!\n");
      }

    }

    // Synthesize the overall linear transformation
    auto linear_trans = steiner_gauss(A, d);
    for (auto gate : linear_trans) ret.push_back(gate);

    return ret;
  }

}
}
