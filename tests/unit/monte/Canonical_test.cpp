#include "autotools.hh"
#include "casm/clexulator/Clexulator.hh"
#include "casm/clexulator/NeighborList.hh"
#include "casm/composition/CompositionCalculator.hh"
#include "casm/composition/CompositionConverter.hh"
#include "casm/system/RuntimeLibrary.hh"
#include "gtest/gtest.h"
#include "testdir.hh"
#include "teststructures.hh"

using namespace CASM;

namespace test {

// /// \brief Construct the StateSamplingFunctionMap
// ///
// /// - This is a container of StateSamplingFunction, which are used to sample
// ///   data if requested by name in SamplingParams.sampler_names.
// /// - Supercell and composition axes information necessary to implement the
// ///   sampling functions is provided via shared `supercell_info`.
// StateSamplingFunctionMap<clexulator::ConfigDoFValues>
// make_sampling_functions(
//     CompositionCalculator const &composition_calculator,
//     CompositionConverter const &composition_converter,
//     clexulator::Correlations &formation_energy_correlations,
//     clexulator::ClusterExpansion &formation_energy_calculator) {
//   typedef monte::State<clexulator::ConfigDoFValues> state_type;
//   typedef monte::StateSamplingFunction<clexulator::ConfigDoFValues>
//       function_type;
//
//   auto comp_n = [=](state_type const &state) {
//     return composition_calculator.mean_num_each_component(
//         state.configuration.occupation);
//   };
//   auto comp = [=](state_type const &state) {
//     return composition_converter.param_composition(comp_n(state));
//   };
//   auto corr = [=](state_type const &state) {
//     formation_energy_correlations.set(&state.configuration);
//     return formation_energy_correlations.intrinsic();
//   };
//   auto formation_energy = [=](state_type const &state) {
//     formation_energy_calculator.set(&state.configuration);
//     return formation_energy_calculator.intrinsic_value();
//   };
//
//   std::vector<function_type> functions = {
//       function_type("comp_n",
//                     "Number of each component (normalized per primitive
//                     cell)", comp_n),
//       function_type("comp", "Parametric composition", comp),
//       function_type("corr", "Correlations (normalized per primitive cell).",
//                     formation_energy),
//       function_type("formation_energy",
//                     "Formation energy of the "
//                     "configuration (normalized per "
//                     "primitive cell).",
//                     formation_energy)};
//
//   monte::StateSamplingFunctionMap<clexulator::ConfigDoFValues> function_map;
//   for (auto const &f : functions) {
//     function_map.insert(f.name, f);
//   }
//   return function_map;
// };

}  // namespace test

TEST(CanonicalTest, Test1) {
  // --- Preperation: copy input files to temporary directory ---
  test::TmpDir testdir;
  testdir.do_not_remove_on_destruction();
  fs::path data_dir = test::data_dir("monte") / "OccClexulatorZrOTest";
  const auto copyOptions =
      fs::copy_options::overwrite_existing | fs::copy_options::recursive;
  fs::copy(data_dir, testdir.path(), copyOptions);
  std::cout << "Testing directory: " << testdir.path() << std::endl;

  // // To run canonical Monte Carlo we need to construct the following input:
  // //
  // // 1. formation_energy_clexulator: Method that calculates formation energy
  // for
  // // a given configuration, and change in formation energy given a
  // configuration
  // // and proposed change in occupation.
  // // 2. state_generator: Method that generates a series of initial states.
  // For
  // // canonical Monte Carlo, a state consists of a composition and a
  // // configuration with that composition. Calculations will be run for the
  // // series of initial states generated by state_generator. Calculations will
  // // stop when the state_generator indicates the series is complete.
  // // 3. sampling_params & samplers: Parameters and methods that specify what
  // // quanties to sample during a single calculation and when they should be
  // // sampled.
  // // 4. completion_check_params: Parameters specifying how to determine when
  // a
  // // single Monte Carlo calculation is complete and how often to check.
  // Includes
  // // parameters for automatic convergence of sampled quantities along with
  // // cutoff parameters to control minimum and maximum run times.
  // // 5. additional params:
  // // - how and where to write results
  // // - how and whether to check for restarts
  // // - random number generator
  // // - customize allowed events
  //

  // --- System ---
  // make prim
  std::shared_ptr<xtal::BasicStructure const> shared_prim =
      std::make_shared<xtal::BasicStructure const>(test::ZrO_prim());

  // --- Supercell ---

  // make supercell transformation matrix, T: S = P * T,
  // S: supercell lattice column matrix
  // P: prim lattice column matrix
  // T: transformation_matrix_to_super

  // clang-format off
  Eigen::Matrix3l transformation_matrix_to_super;
  transformation_matrix_to_super << 9, 0, 0,
                                    0, 9, 0,
                                    0, 0, 9;
  // clang-format on

  // --- Composition calculator & converter ---

  // make composition axes info
  std::vector<std::string> composition_components = {"Zr", "Va", "O"};
  std::vector<std::vector<std::string>> allowed_occs =
      xtal::allowed_molecule_names(*shared_prim);

  // clang-format off
  Eigen::VectorXd composition_axes_origin(3);
  composition_axes_origin << 2.0,  // Zr
                             2.0,  // Va
                             0.0;  // O

  Eigen::MatrixXd composition_axes_end_members(3, 1);
  composition_axes_end_members << 2.0,  // Zr
                                  0.0,  // Va
                                  2.0;  // O
  // clang-format on

  composition::CompositionCalculator composition_calculator(
      composition_components, allowed_occs);
  composition::CompositionConverter composition_converter(
      composition_components, composition_axes_origin,
      composition_axes_end_members);

  // --- Formation energy calculator ---

  // make Clexulator, compiling if necessary...

  // - make empty PrimNeighborList
  //   (to be constructed when making first Clexulator)
  std::shared_ptr<clexulator::PrimNeighborList> prim_neighbor_list;

  // - name of Clexulator source file (excluding .cc extension)
  std::string clexulator_name = "ZrO_Clexulator";

  // - directory where the Clexulator source file is found
  fs::path clexulator_dirpath = testdir.path() / "basis_sets" / "bset.default";

  // - set Clexulator compilation options
  //   ex: g++ -O3 -Wall -fPIC --std=c++17 -I/path/to/include
  std::string clexulator_compile_options =
      //
      // uses $CASM_CXX, else default="g++"
      RuntimeLibrary::default_cxx().first + " " +
      //
      // uses $CASM_CXXFLAGS, else default="-O3 -Wall -fPIC --std=c++17"
      RuntimeLibrary::default_cxxflags().first + " " +
      //
      // uses -I$CASM_INCLUDEDIR,
      //   else -I$CASM_PREFIX/include,
      //   else tries to find "ccasm" or "casm" executable on PATH and looks
      //     for standard include paths relative from there,
      //   else fails with "/not/found"
      include_path(RuntimeLibrary::default_casm_includedir().first) + " " +
      //
      // for testing, adds -I$ABS_SRCDIR/include
      include_path(autotools::abs_includedir());
  std::cout << "compile_options: " << clexulator_compile_options << std::endl;

  // - set Clexulator shared object compilation options
  //   ex: g++ -shared -L/path/to/lib -lcasm_global -lcasm_crystallography
  //     -lcasm_clexulator -lcasm_monte
  std::string clexulator_so_options =
      //
      // uses $CASM_CXX, else default="g++"
      RuntimeLibrary::default_cxx().first + " " +
      //
      // uses $CASM_SOFLAGS, else default="-shared"
      RuntimeLibrary::default_soflags().first + " " +
      //
      // uses -L$CASM_LIBDIR,
      //   else -L$CASM_PREFIX/lib,
      //   else tries to find "ccasm" or "casm" executables on PATH and looks
      //     for libcasm at standard relative paths from there,
      //   else fails with "-L/not/found"
      link_path(RuntimeLibrary::default_casm_libdir().first) + " " +
      //
      // for testing, adds -L$ABS_TOP_BUILDDIR/.libs
      link_path(autotools::abs_libdir()) + " " +
      //
      // requires libcasm_clexulator:
      "-lcasm_clexulator ";
  std::cout << "so_options: " << clexulator_so_options << std::endl;

  //   notes:
  //   - The prim_neighbor_list will be constructed / expanded as necessary
  //   - Standard practice if >1 clexulator is needed is that the
  //     prim_neighbor_list will be the same for all clexulator. However, this
  //     is not strictly necessary. If clexulator require different
  //     PrimNeighborList, then different SuperNeighborList are also required
  //     to be contructed from each PrimNeighborList.
  std::shared_ptr<clexulator::Clexulator> formation_energy_clexulator =
      std::make_shared<clexulator::Clexulator>(clexulator::make_clexulator(
          clexulator_name, clexulator_dirpath, prim_neighbor_list,
          clexulator_compile_options, clexulator_so_options));
  EXPECT_EQ(formation_energy_clexulator->corr_size(), 74);

  // // - make a shared SuperNeighborList
  // //   - this step must be done after constructing all Clexulator that will
  // //     be used so that the prim_neighbor_list is complete
  // std::shared_ptr<SuperNeighborList> supercell_neighbor_list =
  //     std::make_shared<SuperNeighborList>(transformation_matrix_to_super,
  //                                         *prim_neighbor_list);
  //
  // // - make formation energy eci (TODO: read from file)
  // clexulator::SparseCoefficients formation_energy_eci;
  // formation_energy_eci.index = std::vector<unsigned int>({0, 1, 2});
  // formation_energy_eci.value = std::vector<double>({0.0, -0.1, 0.1});
  //
  // // - make a clexulator::Correlations object
  // clexulator::Correlations formation_energy_correlations(
  //     supercell_neighbor_list, formation_energy_clexulator,
  //     formation_energy_eci.index);
  //
  // // make formation energy calculator
  // clexulator::ClusterExpansion formation_energy_calculator(
  //     formation_energy_correlations, formation_energy_eci);
  //
  // // --- Sampling ---
  // // construct sampling params
  // monte::SamplingParams sampling_params;
  //
  // // - linear sampling: sample when count/time == a + b*sample_index
  // //   - default: choice of {a=0, b=1} results in sampling after every pass
  // //   - ex: choice of {a=10, b=2} results in sampling after every 2 passes,
  // //     starting with the tenth pass
  // sampling_params.sample_mode = SAMPLE_MODE::BY_PASS;
  // sampling_params.sample_method = SAMPLE_METHOD::LINEAR;
  // sampling_params.count_sampling_params = {0, 1};  // {a=0, b=1}
  //
  // // - also possible is log sampling:
  // //   sample when count/time = a + b^(sample_index-c)
  // //   - note: log sampling is typically most useful for understanding KMC
  // //     calculations with time sampling
  // //   - ex: choice of {a=0, b=10, c=0} results in sampling at count
  // //     1, 10, 100, etc.
  // //   - ex: choice of {a=0.0, b=10.0, c=1.0} results in sampling at time
  // //     0.1, 1.0, 10.0, etc.
  // // sampling_params.sample_mode = SAMPLE_MODE::BY_PASS;
  // // sampling_params.sample_method = SAMPLE_METHOD::LOG;
  // // sampling_params.count_sampling_params = {0, 10, 0}; // {a=0, b=10, c=0}
  //
  // // - what to sample
  // // - notes:
  // //   - names must match StateSamplingFunction in `sampling_functions`
  // // - TODO:
  // //   - support "index expresions"?
  // //   - should this be in SamplingParams?
  // //   - should `samplers` be in SamplingParams?
  // //     (prefer not in order to reduce templating)
  // sampling_params.sampler_names = {"comp_n", "comp", "corr",
  //                                  "formation_energy"};
  //
  // // - also sample the overal trajectory (saves a copy of the entire
  // //   configuration at the sample time)
  // sampling_params.sample_trajectory = true;
  //
  // // construct state samplers:
  // StateSamplingFunctionMap<clexulator::ConfigDoFValues> sampling_functions =
  //     make_sampling_functions(composition_calculator, composition_converter,
  //                             formation_energy_correlations,
  //                             formation_energy_calculator);
  //
  // std::map<std::string, monte::StateSampler<clexulator::ConfigDoFValues>>
  //     samplers = make_state_samplers(sampling_functions,
  //                                    sampling_params.sampler_names);
  //
  // // --- State generation ---
  //
  // // - make initial configuration:
  // std::map<DoFKey, GlobalDoFSetType> global_dof_info =
  //     clexulator::make_global_dof_info(*shared_prim);
  // std::map<DoFKey, std::vector<LocalDoFSetType>> local_dof_info =
  //     clexulator::make_local_dof_info(*shared_prim);
  // clexulator::ConfigDoFValues initial_configuration =
  //     clexulator::make_default_config_dof_values(
  //         shared_prim->basis().size(),
  //         transformation_matrix_to_super.determinant(), prim_global_dof_info,
  //         prim_local_dof_info);
  //
  // // - make initial conditions:
  // monte::VectorValueMap initial_conditions = make_canonical_conditions(
  //     300.0,                   // temperature (K)
  //     composition_components,  // composition vector order
  //     {{"Zr", 2.0},            // composition values (#/unit cell)
  //      {"O", 0.01},
  //      {"Va", 1.99}});
  //
  // // - make initial state:
  // monte::State<clexulator::ConfigDoFValues>
  // initial_state(initial_configuration,
  //                                                         initial_conditions);
  //
  // // - make conditions increment:
  // monte::VectorValueMap conditions_increment = make_canonical_conditions(
  //     0.0,                     // delta temperature (K)
  //     composition_components,  // composition vector order
  //     {{"Zr", 0.0},            // delta composition values (#/unit cell)
  //      {"O", 0.01},
  //      {"Va", -0.01}});
  //
  // // - number of states to generate and perform Monte Carlo calculations for:
  // Index n_states = 10;
  //
  // // - dependent_runs: If true, after the first state, each subsequent state
  // // generated will use the final configuration of the previous calculation
  // for
  // // its initial configuration; If false, always use the original initial
  // // configuration
  // bool dependent_runs = true;
  //
  // // - make state_generator:
  // monte::IncrementalConditionsStateGenerator<clexulator::ConfigDoFValues>
  //     state_generator(initial_state, conditions_increment, n_states,
  //                     dependent_runs);
  //
  // // --- Completion Checking ---
  //
  // monte::CompletionCheckParams completion_check_params;
  //
  // // - Set automatic convergence parameters:
  // completion_check_params.insert(
  //     converge(samplers,
  //     "formation_energy").precision(0.001).confidence(0.95),
  //     converge(samplers, "comp_n").component("O").precision(0.001),
  //     converge(samplers, "comp_n").component("Va").precision(0.01),
  //     converge(samplers, "corr").component_index(1).precision(0.001),
  //     converge(samplers, "corr").component_index(2).precision(0.001),
  //     converge(samplers, "comp").component("a").precision(0.01));
  //
  // // - Check for completion performed if:
  // //       n_samples % check_frequency == 0 && n_samples >= check_begin
  // completion_check_params.check_begin = 10;
  // completion_check_params.check_frequency = 10;
  //
  // // - Set cutoffs: Minimum and maximum number of passes:
  // completion_check_params.cutoff_params.min_count = 100;
  // completion_check_params.cutoff_params.max_count = 1000000;
  //
  // monte::CompletionCheck completion_check(completion_check_params);
  //
  // // --- Results IO ---
  // // TODO: results_io_params: where and how to write results, look for
  // restart
  // // data, etc.
  //
  // // --- Running canonical Monte Carlo ---
  // // CanonicalMonteCarloResults results = canonical_monte_carlo(
  // //     formation_energy_calculator, sampling_params, samplers,
  // //     results_io_params, state_generator, completion_check, mtrand);

  EXPECT_EQ(1, 1);
}
