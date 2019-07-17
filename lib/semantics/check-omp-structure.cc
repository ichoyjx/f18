// Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "check-omp-structure.h"
#include "tools.h"
#include "../parser/parse-tree.h"

namespace Fortran::semantics {

static constexpr OmpDirectiveSet doSet{OmpDirective::DO,
    OmpDirective::PARALLEL_DO, OmpDirective::DO_SIMD,
    OmpDirective::PARALLEL_DO_SIMD};
static constexpr OmpDirectiveSet simdSet{
    OmpDirective::SIMD, OmpDirective::DO_SIMD, OmpDirective::PARALLEL_DO_SIMD};
static constexpr OmpDirectiveSet doSimdSet{
    OmpDirective::DO_SIMD, OmpDirective::PARALLEL_DO_SIMD};

std::string OmpStructureChecker::ContextDirectiveAsFortran() {
  auto dir{EnumToString(GetContext().directive)};
  std::replace(dir.begin(), dir.end(), '_', ' ');
  return dir;
}

bool OmpStructureChecker::HasInvalidWorksharingNesting(
    const parser::CharBlock &source, const OmpDirectiveSet &set) {
  // set contains all the invalid closely nested directives
  // for the given directive (`source` here)
  if (CurrentDirectiveIsNested() && set.test(GetContext().directive)) {
    context_.Say(source,
        "A worksharing region may not be closely nested inside a "
        "worksharing, explicit task, taskloop, critical, ordered, atomic, or "
        "master region."_err_en_US);
    return true;
  }
  return false;
}

void OmpStructureChecker::CheckAllowed(const OmpClause &type) {
  if (!GetContext().allowedClauses.test(type) &&
      !GetContext().allowedOnceClauses.test(type)) {
    context_.Say(GetContext().clauseSource,
        "%s clause is not allowed on the %s directive"_err_en_US,
        EnumToString(type),
        parser::ToUpperCaseLetters(GetContext().directiveSource.ToString()));
    return;
  }
  if (GetContext().allowedOnceClauses.test(type) && FindClause(type)) {
    context_.Say(GetContext().clauseSource,
        "At most one %s clause can appear on the %s directive"_err_en_US,
        EnumToString(type),
        parser::ToUpperCaseLetters(GetContext().directiveSource.ToString()));
    return;
  }
  SetContextClauseInfo(type);
}

void OmpStructureChecker::Enter(const parser::Block &block) {
  parser::CharBlock dirSource{nullptr};
  parser::CharBlock endDirSource{nullptr};
  auto loopDirIter{block.end()};
  auto endLoopDirIter{block.end()};
  auto doConstructIter{block.end()};
  for (auto i{block.begin()}, end{block.end()}; i != end; ++i) {
    if (const auto *exec{std::get_if<parser::ExecutableConstruct>(&i->u)}) {
      if (const auto *ompCons{
              std::get_if<common::Indirection<parser::OpenMPConstruct>>(
                  &exec->u)}) {
        if (const auto *ompLoop{std::get_if<parser::OpenMPLoopConstruct>(
                &ompCons->value().u)}) {
          const auto &dir{std::get<parser::OmpLoopDirective>(ompLoop->t)};
          loopDirIter = i;
          dirSource = dir.source;
        }
      }
      if (const auto *doCons{
              std::get_if<common::Indirection<parser::DoConstruct>>(
                  &exec->u)}) {
        doConstructIter = i;
        if (loopDirIter != end && i != ++loopDirIter) {
          context_.Say(dirSource,
              "Do loop is expected after the %s directive"_err_en_US,
              parser::ToUpperCaseLetters(dirSource.ToString()));
          loopDirIter = end;
        }
      }
      if (const auto *endDir{
              std::get_if<common::Indirection<parser::OpenMPEndLoopDirective>>(
                  &exec->u)}) {
        // const auto
        // *endLoopDir{std::get_if<parser::OmpLoopDirective>(&endDir->value().u)};
        if (doConstructIter == end || i != ++doConstructIter) {
          context_.Say(endDir->value().source,
              "The %s must follow the DO loop associated with the "
              "loop construct"_err_en_US,
              parser::ToUpperCaseLetters(endDir->value().source.ToString()));
          doConstructIter = end;
        }
      }
    }
  }  // Block list
}

void OmpStructureChecker::Enter(const parser::OpenMPConstruct &x) {
  // 2.8.1 TODO: Simd Construct with Ordered Construct Nesting check
}

void OmpStructureChecker::Enter(const parser::OpenMPLoopConstruct &x) {
  const auto &dir{std::get<parser::OmpLoopDirective>(x.t)};
  if (dir.v == parser::OmpLoopDirective::Directive::Do) {
    HasInvalidWorksharingNesting(dir.source,
        {OmpDirective::DO, OmpDirective::SECTIONS, OmpDirective::SINGLE,
            OmpDirective::WORKSHARE, OmpDirective::TASK, OmpDirective::TASKLOOP,
            OmpDirective::CRITICAL, OmpDirective::ORDERED, OmpDirective::ATOMIC,
            OmpDirective::MASTER});
  }

  // push a context even in the error case
  OmpContext ct{dir.source};
  ompContext_.push_back(ct);
}

void OmpStructureChecker::Leave(const parser::OpenMPLoopConstruct &) {
  ompContext_.pop_back();
}

void OmpStructureChecker::Enter(const parser::OpenMPBlockConstruct &x) {
  const auto &dir{std::get<parser::OmpBlockDirective>(x.t)};
  OmpContext ct{dir.source};
  ompContext_.push_back(ct);
}

void OmpStructureChecker::Leave(const parser::OpenMPBlockConstruct &) {
  ompContext_.pop_back();
}

void OmpStructureChecker::Enter(const parser::OmpBlockDirective &x) {
  switch (x.v) {
  // 2.5 parallel-clause -> if-clause |
  //                        num-threads-clause |
  //                        default-clause |
  //                        private-clause |
  //                        firstprivate-clause |
  //                        shared-clause |
  //                        copyin-clause |
  //                        reduction-clause |
  //                        proc-bind-clause
  case parser::OmpBlockDirective::Directive::Parallel: {
    // reserve for nesting check
    SetContextDirectiveEnum(OmpDirective::PARALLEL);
    OmpClauseSet allowed{OmpClause::DEFAULT, OmpClause::PRIVATE,
        OmpClause::FIRSTPRIVATE, OmpClause::SHARED, OmpClause::COPYIN,
        OmpClause::REDUCTION};
    SetContextAllowed(allowed);
    OmpClauseSet allowedOnce{
        OmpClause::IF, OmpClause::NUM_THREADS, OmpClause::PROC_BIND};
    SetContextAllowedOnce(allowedOnce);
  } break;
  default:
    // TODO others
    break;
  }
}

void OmpStructureChecker::Enter(const parser::OmpLoopDirective &x) {
  switch (x.v) {
  // 2.7.1 do-clause -> private-clause |
  //                    firstprivate-clause |
  //                    lastprivate-clause |
  //                    linear-clause |
  //                    reduction-clause |
  //                    schedule-clause |
  //                    collapse-clause |
  //                    ordered-clause
  case parser::OmpLoopDirective::Directive::Do: {
    // reserve for nesting check
    SetContextDirectiveEnum(OmpDirective::DO);
    OmpClauseSet allowed{OmpClause::PRIVATE, OmpClause::FIRSTPRIVATE,
        OmpClause::LASTPRIVATE, OmpClause::LINEAR, OmpClause::REDUCTION};
    SetContextAllowed(allowed);
    OmpClauseSet allowedOnce{
        OmpClause::SCHEDULE, OmpClause::COLLAPSE, OmpClause::ORDERED};
    SetContextAllowedOnce(allowedOnce);
  } break;

  // 2.11.1 parallel-do-clause -> parallel-clause |
  //                              do-clause
  case parser::OmpLoopDirective::Directive::ParallelDo: {
    SetContextDirectiveEnum(OmpDirective::PARALLEL_DO);
    OmpClauseSet allowed{OmpClause::DEFAULT, OmpClause::PRIVATE,
        OmpClause::FIRSTPRIVATE, OmpClause::SHARED, OmpClause::COPYIN,
        OmpClause::REDUCTION, OmpClause::LASTPRIVATE, OmpClause::LINEAR};
    SetContextAllowed(allowed);
    OmpClauseSet allowedOnce{OmpClause::IF, OmpClause::NUM_THREADS,
        OmpClause::PROC_BIND, OmpClause::SCHEDULE, OmpClause::COLLAPSE,
        OmpClause::ORDERED};
    SetContextAllowedOnce(allowedOnce);
  } break;

  // 2.8.1 simd-clause -> safelen-clause |
  //                      simdlen-clause |
  //                      linear-clause |
  //                      aligned-clause |
  //                      private-clause |
  //                      lastprivate-clause |
  //                      reduction-clause |
  //                      collapse-clause
  case parser::OmpLoopDirective::Directive::Simd: {
    SetContextDirectiveEnum(OmpDirective::SIMD);
    OmpClauseSet allowed{OmpClause::LINEAR, OmpClause::ALIGNED,
        OmpClause::PRIVATE, OmpClause::LASTPRIVATE, OmpClause::REDUCTION};
    SetContextAllowed(allowed);
    OmpClauseSet allowedOnce{
        OmpClause::COLLAPSE, OmpClause::SAFELEN, OmpClause::SIMDLEN};
    SetContextAllowedOnce(allowedOnce);
  } break;

  // 2.8.3 do-simd-clause -> do-clause |
  //                         simd-clause
  case parser::OmpLoopDirective::Directive::DoSimd: {
    SetContextDirectiveEnum(OmpDirective::DO_SIMD);
    OmpClauseSet allowed{OmpClause::PRIVATE, OmpClause::FIRSTPRIVATE,
        OmpClause::LASTPRIVATE, OmpClause::LINEAR, OmpClause::REDUCTION,
        OmpClause::ALIGNED};
    SetContextAllowed(allowed);
    OmpClauseSet allowedOnce{OmpClause::SCHEDULE, OmpClause::COLLAPSE,
        OmpClause::ORDERED, OmpClause::SAFELEN, OmpClause::SIMDLEN};
    SetContextAllowedOnce(allowedOnce);
  } break;

  // 2.11.4 parallel-do-simd-clause -> parallel-clause |
  //                                   do-simd-clause
  case parser::OmpLoopDirective::Directive::ParallelDoSimd: {
    SetContextDirectiveEnum(OmpDirective::PARALLEL_DO_SIMD);
    OmpClauseSet allowed{OmpClause::DEFAULT, OmpClause::PRIVATE,
        OmpClause::FIRSTPRIVATE, OmpClause::SHARED, OmpClause::COPYIN,
        OmpClause::REDUCTION, OmpClause::LASTPRIVATE, OmpClause::LINEAR,
        OmpClause::ALIGNED};
    SetContextAllowed(allowed);
    OmpClauseSet allowedOnce{OmpClause::IF, OmpClause::NUM_THREADS,
        OmpClause::PROC_BIND, OmpClause::SCHEDULE, OmpClause::COLLAPSE,
        OmpClause::ORDERED, OmpClause::SAFELEN, OmpClause::SIMDLEN};
    SetContextAllowedOnce(allowedOnce);
  } break;

  default:
    // TODO others
    break;
  }
}

void OmpStructureChecker::Leave(const parser::OmpClauseList &) {
  // 2.7 Loop Construct Restriction
  if (doSet.test(GetContext().directive)) {
    if (auto *clause{FindClause(OmpClause::SCHEDULE)}) {
      // only one schedule clause is allowed
      const auto &schedClause{std::get<parser::OmpScheduleClause>(clause->u)};
      if (ScheduleModifierHasType(schedClause,
              parser::OmpScheduleModifierType::ModType::Nonmonotonic)) {
        if (FindClause(OmpClause::ORDERED)) {
          context_.Say(clause->source,
              "The NONMONOTONIC modifier cannot be specified "
              "if an ORDERED clause is specified"_err_en_US);
        }
        if (ScheduleModifierHasType(schedClause,
                parser::OmpScheduleModifierType::ModType::Monotonic)) {
          context_.Say(clause->source,
              "The MONOTONIC and NONMONOTONIC modifiers "
              "cannot be both specified"_err_en_US);
        }
      }
    }

    if (auto *clause{FindClause(OmpClause::ORDERED)}) {
      // only one ordered clause is allowed
      const auto &orderedClause{
          std::get<parser::OmpClause::Ordered>(clause->u)};

      if (orderedClause.v.has_value()) {
        if (FindClause(OmpClause::LINEAR)) {
          context_.Say(clause->source,
              "A loop directive may not have both a LINEAR clause and "
              "an ORDERED clause with a parameter"_err_en_US);
        }

        if (auto *clause2{FindClause(OmpClause::COLLAPSE)}) {
          const auto &collapseClause{
              std::get<parser::OmpClause::Collapse>(clause2->u)};
          // ordered and collapse both have parameters
          if (const auto orderedValue{GetIntValue(orderedClause.v)}) {
            if (const auto collapseValue{GetIntValue(collapseClause.v)}) {
              if (*orderedValue > 0 && *orderedValue < *collapseValue) {
                context_.Say(clause->source,
                    "The parameter of the ORDERED clause must be "
                    "greater than or equal to "
                    "the parameter of the COLLAPSE clause"_err_en_US);
              }
            }
          }
        }
      }

      // TODO: ordered region binding check (requires nesting implementation)
    }
  }  // doSet

  // 2.8.1 Simd Construct Restriction
  if (simdSet.test(GetContext().directive)) {
    if (auto *clause{FindClause(OmpClause::SIMDLEN)}) {
      if (auto *clause2{FindClause(OmpClause::SAFELEN)}) {
        const auto &simdlenClause{
            std::get<parser::OmpClause::Simdlen>(clause->u)};
        const auto &safelenClause{
            std::get<parser::OmpClause::Safelen>(clause2->u)};
        // simdlen and safelen both have parameters
        if (const auto simdlenValue{GetIntValue(simdlenClause.v)}) {
          if (const auto safelenValue{GetIntValue(safelenClause.v)}) {
            if (*safelenValue > 0 && *simdlenValue > *safelenValue) {
              context_.Say(clause->source,
                  "The parameter of the SIMDLEN clause must be less than or "
                  "equal to the parameter of the SAFELEN clause"_err_en_US);
            }
          }
        }
      }
    }

    // TODO: A list-item cannot appear in more than one aligned clause
  }  // SIMD
}

void OmpStructureChecker::Enter(const parser::OmpClause &x) {
  SetContextClause(x);
}

void OmpStructureChecker::Enter(const parser::OmpClause::Defaultmap &) {
  CheckAllowed(OmpClause::DEFAULTMAP);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Inbranch &) {
  CheckAllowed(OmpClause::INBRANCH);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Mergeable &) {
  CheckAllowed(OmpClause::MERGEABLE);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Nogroup &) {
  CheckAllowed(OmpClause::NOGROUP);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Notinbranch &) {
  CheckAllowed(OmpClause::NOTINBRANCH);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Untied &) {
  CheckAllowed(OmpClause::UNTIED);
}

void OmpStructureChecker::Enter(const parser::OmpClause::Collapse &x) {
  CheckAllowed(OmpClause::COLLAPSE);
  // collapse clause must have a parameter
  if (const auto v{GetIntValue(x.v)}) {
    if (*v <= 0) {
      context_.Say(GetContext().clauseSource,
          "The parameter of the COLLAPSE clause must be "
          "a constant positive integer expression"_err_en_US);
    }
  }
}

void OmpStructureChecker::Enter(const parser::OmpClause::Copyin &) {
  CheckAllowed(OmpClause::COPYIN);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Copyprivate &) {
  CheckAllowed(OmpClause::COPYPRIVATE);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Device &) {
  CheckAllowed(OmpClause::DEVICE);
}
void OmpStructureChecker::Enter(const parser::OmpClause::DistSchedule &) {
  CheckAllowed(OmpClause::DIST_SCHEDULE);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Final &) {
  CheckAllowed(OmpClause::FINAL);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Firstprivate &) {
  CheckAllowed(OmpClause::FIRSTPRIVATE);
}
void OmpStructureChecker::Enter(const parser::OmpClause::From &) {
  CheckAllowed(OmpClause::FROM);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Grainsize &) {
  CheckAllowed(OmpClause::GRAINSIZE);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Lastprivate &) {
  CheckAllowed(OmpClause::LASTPRIVATE);
}
void OmpStructureChecker::Enter(const parser::OmpClause::NumTasks &) {
  CheckAllowed(OmpClause::NUM_TASKS);
}
void OmpStructureChecker::Enter(const parser::OmpClause::NumTeams &) {
  CheckAllowed(OmpClause::NUM_TEAMS);
}
void OmpStructureChecker::Enter(const parser::OmpClause::NumThreads &x) {
  CheckAllowed(OmpClause::NUM_THREADS);
  if (const auto v{GetIntValue(x.v)}) {
    if (*v <= 0) {
      context_.Say(GetContext().clauseSource,
          "The parameter of the NUM_THREADS clause must be "
          "a positive integer expression"_err_en_US);
    }
  }
  // if parameter is variable, defer to Expression Analysis
}

void OmpStructureChecker::Enter(const parser::OmpClause::Ordered &x) {
  CheckAllowed(OmpClause::ORDERED);
  // the parameter of ordered clause is optional
  if (const auto &expr{x.v}) {
    if (const auto v{GetIntValue(expr)}) {
      if (*v <= 0) {
        context_.Say(GetContext().clauseSource,
            "The parameter of the ORDERED clause must be "
            "a constant positive integer expression"_err_en_US);
      }
    }

    // 2.8.3 Loop SIMD Construct Restriction
    if (doSimdSet.test(GetContext().directive)) {
      context_.Say(GetContext().clauseSource,
          "No ORDERED clause with a parameter can be specified "
          "on the %s directive"_err_en_US,
          ContextDirectiveAsFortran());
    }
  }
}
void OmpStructureChecker::Enter(const parser::OmpClause::Priority &) {
  CheckAllowed(OmpClause::PRIORITY);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Private &) {
  CheckAllowed(OmpClause::PRIVATE);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Safelen &x) {
  CheckAllowed(OmpClause::SAFELEN);

  if (const auto v{GetIntValue(x.v)}) {
    if (*v <= 0) {
      context_.Say(GetContext().clauseSource,
          "The parameter of the SAFELEN clause must be "
          "a constant positive integer expression"_err_en_US);
    }
  }
}
void OmpStructureChecker::Enter(const parser::OmpClause::Shared &) {
  CheckAllowed(OmpClause::SHARED);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Simdlen &x) {
  CheckAllowed(OmpClause::SIMDLEN);

  if (const auto v{GetIntValue(x.v)}) {
    if (*v <= 0) {
      context_.Say(GetContext().clauseSource,
          "The parameter of the SIMDLEN clause must be "
          "a constant positive integer expression"_err_en_US);
    }
  }
}
void OmpStructureChecker::Enter(const parser::OmpClause::ThreadLimit &) {
  CheckAllowed(OmpClause::THREAD_LIMIT);
}
void OmpStructureChecker::Enter(const parser::OmpClause::To &) {
  CheckAllowed(OmpClause::TO);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Link &) {
  CheckAllowed(OmpClause::LINK);
}
void OmpStructureChecker::Enter(const parser::OmpClause::Uniform &) {
  CheckAllowed(OmpClause::UNIFORM);
}
void OmpStructureChecker::Enter(const parser::OmpClause::UseDevicePtr &) {
  CheckAllowed(OmpClause::USE_DEVICE_PTR);
}
void OmpStructureChecker::Enter(const parser::OmpClause::IsDevicePtr &) {
  CheckAllowed(OmpClause::IS_DEVICE_PTR);
}

void OmpStructureChecker::Enter(const parser::OmpAlignedClause &x) {
  CheckAllowed(OmpClause::ALIGNED);

  if (const auto &expr{
          std::get<std::optional<parser::ScalarIntConstantExpr>>(x.t)}) {
    if (const auto v{GetIntValue(*expr)}) {
      if (*v <= 0) {
        context_.Say(GetContext().clauseSource,
            "The ALIGNMENT parameter of the ALIGNED clause must be "
            "a constant positive integer expression"_err_en_US);
      }
    }
  }
  // 2.8.1 TODO: list-item attribute check
}
void OmpStructureChecker::Enter(const parser::OmpDefaultClause &) {
  CheckAllowed(OmpClause::DEFAULT);
}
void OmpStructureChecker::Enter(const parser::OmpDependClause &) {
  CheckAllowed(OmpClause::DEPEND);
}
void OmpStructureChecker::Enter(const parser::OmpIfClause &) {
  CheckAllowed(OmpClause::IF);
}
void OmpStructureChecker::Enter(const parser::OmpLinearClause &x) {
  CheckAllowed(OmpClause::LINEAR);

  // 2.7 Loop Construct Restriction
  if ((doSet | simdSet).test(GetContext().directive)) {
    if (std::holds_alternative<parser::OmpLinearClause::WithModifier>(x.u)) {
      context_.Say(GetContext().clauseSource,
          "A modifier may not be specified in a LINEAR clause "
          "on the %s directive"_err_en_US,
          ContextDirectiveAsFortran());
    }
  }
}
void OmpStructureChecker::Enter(const parser::OmpMapClause &) {
  CheckAllowed(OmpClause::MAP);
}
void OmpStructureChecker::Enter(const parser::OmpProcBindClause &) {
  CheckAllowed(OmpClause::PROC_BIND);
}
void OmpStructureChecker::Enter(const parser::OmpReductionClause &) {
  CheckAllowed(OmpClause::REDUCTION);
}

bool OmpStructureChecker::ScheduleModifierHasType(
    const parser::OmpScheduleClause &x,
    const parser::OmpScheduleModifierType::ModType &type) {
  const auto &modifier{
      std::get<std::optional<parser::OmpScheduleModifier>>(x.t)};
  if (modifier.has_value()) {
    const auto &modType1{
        std::get<parser::OmpScheduleModifier::Modifier1>(modifier->t)};
    const auto &modType2{
        std::get<std::optional<parser::OmpScheduleModifier::Modifier2>>(
            modifier->t)};
    if (modType1.v.v == type ||
        (modType2.has_value() && modType2->v.v == type)) {
      return true;
    }
  }
  return false;
}
void OmpStructureChecker::Enter(const parser::OmpScheduleClause &x) {
  CheckAllowed(OmpClause::SCHEDULE);

  // 2.7 Loop Construct Restriction
  if (doSet.test(GetContext().directive)) {
    const auto &kind{std::get<1>(x.t)};
    const auto &chunk{std::get<2>(x.t)};
    if (chunk.has_value()) {
      if (kind == parser::OmpScheduleClause::ScheduleType::Runtime ||
          kind == parser::OmpScheduleClause::ScheduleType::Auto) {
        context_.Say(GetContext().clauseSource,
            "When SCHEDULE clause has %s specified, "
            "it must not have chunk size specified"_err_en_US,
            parser::ToUpperCaseLetters(
                parser::OmpScheduleClause::EnumToString(kind)));
      }
    }

    if (ScheduleModifierHasType(
            x, parser::OmpScheduleModifierType::ModType::Nonmonotonic)) {
      if (kind != parser::OmpScheduleClause::ScheduleType::Dynamic &&
          kind != parser::OmpScheduleClause::ScheduleType::Guided) {
        context_.Say(GetContext().clauseSource,
            "The NONMONOTONIC modifier can only be specified with "
            "SCHEDULE(DYNAMIC) or SCHEDULE(GUIDED)"_err_en_US);
      }
    }
  }
}
}
