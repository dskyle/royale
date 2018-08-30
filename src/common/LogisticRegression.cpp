#include "royale/Trial.hpp"

#include <mlpack/methods/logistic_regression/logistic_regression.hpp>

using namespace mlpack::regression;
using LogReg = LogisticRegression<>;

namespace royale {

AnalysisOutput::Enum AnalysisType::LogisticRegression::do_analysis(
    const AnalysisInput &input,
    io::yield_context)
{
  auto log = spdlog::get("log");

  AnalysisOutput::LogisticRegression::preds_type preds;
  std::vector<std::pair<const TrialInput*, const TrialOutput*>> trials;
  for (const auto &trial : input.data()) {
    SPDLOG_TRACE(log, "Examining trial: {}", xtd::lazy_json_dump(trial));
    trial.status().visit(xtd::overload(
      [&](const TrialStatus::Complete &complete) {
        trials.emplace_back(&trial.input(), &complete.output());
        for (const auto &pred : complete.output().preds()) {
          SPDLOG_TRACE(log, "Examining predicate: {}",
              xtd::lazy_json_dump(pred));
          auto &cur = preds[pred.first];
          if (pred.second) {
            SPDLOG_TRACE(log, "Predicate {} is sat", pred.first);
            cur.add_sat();
          } else {
            SPDLOG_TRACE(log, "Predicate {} is unsat", pred.first);
            cur.add_unsat();
          }
        }
      },
      [&](const TrialStatus &) {
        // Ignore incomplete trials
      }
    ));
  }
  SPDLOG_TRACE(log, "LogisticRegression: found {} trials", trials.size());
  if (trials.size() > 0) {
    arma::mat inputs(trials.front().first->sample().size(), trials.size());

    SPDLOG_TRACE(log, "LogisticRegression: building input matrix ({}x{})",
        inputs.n_rows, inputs.n_cols);
    size_t col = 0;
    for (const auto &trial : trials) {
      size_t row = 0;
      for (const auto &sample : trial.first->sample()) {
        inputs(row, col) = xtd::dbl(sample.second);
        ++row;
      }
      ++col;
    }

    for (auto &pred : preds) {
      arma::Row<size_t> outputs(trials.size());
      SPDLOG_TRACE(log, "LogisticRegression: building output matrix (1x{}) "
          "for predicate {}",
          outputs.n_cols, pred.first);
      size_t col = 0;
      for (const auto &trial : trials) {
        outputs(col) = trial.second->preds().at(pred.first);
        ++col;
      }

      LogReg regress(inputs, outputs);

      const auto &params = regress.Parameters();

      LogisticPredicateOutput::coeffs_type lpo_coeffs;

      lpo_coeffs[""] = params[0];
      col = 1;
      for (const auto &sample : trials.front().first->sample()) {
        lpo_coeffs[sample.first] = params[col];
        ++col;
      }
      pred.second.coeffs(std::move(lpo_coeffs));
    }
  }
  return output_type::mk(std::move(preds));
}

}
