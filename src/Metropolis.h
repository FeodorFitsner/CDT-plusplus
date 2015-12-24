/// Causal Dynamical Triangulations in C++ using CGAL
///
/// Copyright (c) 2015 Adam Getchell
///
/// Performs the Metropolis-Hastings algorithm on the foliated Delaunay
/// triangulations.
/// For details see:
/// M. Creutz, and B. Freedman. “A Statistical Approach to Quantum Mechanics.”
/// Annals of Physics 132 (1981): 427–62.
/// http://thy.phy.bnl.gov/~creutz/mypubs/pub044.pdf

/// \done Initialization
/// \done operator()
/// \done CalculateA1
/// \done CalculateA2
/// \todo Update N1_TL_, N3_31_ and N3_22_ after successful moves
/// \todo Implement 3D Metropolis algorithm in operator()
/// \todo Implement concurrency

/// @file Metropolis.h
/// @brief Perform Metropolis-Hasting algorithm on Delaunay Triangulations
/// @author Adam Getchell
/// @bug Does not quite keep track of (2,3), (3,2)

#ifndef SRC_METROPOLIS_H_
#define SRC_METROPOLIS_H_

// CGAL headers
// #include <CGAL/Gmpzf.h>
// #include <CGAL/Gmpz.h>
// #include <mpfr.h>
// #include <CGAL/Mpzf.h>

// CDT headers
#include "S3ErgodicMoves.h"
#include "S3Action.h"

// C++ headers
#include <vector>
#include <utility>
#include <tuple>

using Gmpzf = CGAL::Gmpzf;
// using Gmpz = CGAL::Gmpz;
// using MP_Float = CGAL::MP_Float;
// using move_tuple = std::tuple<std::atomic<long int>,
//                               std::atomic<long int>,
//                               std::atomic<long int>,
//                               std::atomic<long int>>;
using move_tuple = std::tuple<uintmax_t,
                              uintmax_t,
                              uintmax_t,
                              uintmax_t,
                              uintmax_t>;

extern const unsigned PRECISION;

enum class move_type {TWO_THREE = 0,
                      THREE_TWO = 1,
                      TWO_SIX = 2,
                      SIX_TWO = 3,
                      FOUR_FOUR = 4};

// template <typename T1, typename T2>
// auto attempt_23_move(T1&& universe_ptr, T2&& simplex_types) noexcept
//                      -> decltype(universe_ptr) {
//   return universe_ptr;
// }  // attempt_23_move()

/// @class Metropolis
///
/// @brief Metropolis-Hastings algorithm functor
///
/// The Metropolis-Hastings algorithm is a Markov Chain Monte Carlo method.
/// The probability of making an ergodic (Pachner) move is:
///
/// \f[P_{ergodic move}=a_{1}a_{2}\f]
/// \f[a_1=\frac{move[i]}{\sum\limits_{i}move[i]}\f]
/// \f[a_2=e^{\Delta S}\f]
class Metropolis {
 public:
  /// @brief Metropolis constructor
  ///
  /// Very minimal setup of runtime job parameters.
  /// All the real work is done by operator().
  ///
  /// @param[in] Alpha  \f$\alpha\f$ is the timelike edge length
  /// @param[in] K      \f$k=\frac{1}{8\pi G_{Newton}}\f$
  /// @param[in] Lambda \f$\lambda=k*\Lambda\f$ where \f$\Lambda\f$ is the
  ///                   Cosmological constant
  /// @param[in] passes Number of passes of ergodic moves on triangulation.
  /// @param[in] output_every_n_passes How often to print/write output.
  Metropolis(const long double Alpha,
             const long double K,
             const long double Lambda,
             const unsigned passes,
             const unsigned output_every_n_passes)
             : Alpha_(Alpha),
               K_(K),
               Lambda_(Lambda),
               passes_(passes),
               output_every_n_passes_(output_every_n_passes) {
    #ifndef NDEBUG
    std::cout << "Ctor called." << std::endl;
    #endif
  }

  /// Gets value of **Alpha_**.
  auto Alpha() const {return Alpha_;}

  /// Gets value of **K_**.
  auto K() const {return K_;}

  /// Gets value of **Lambda_**.
  auto Lambda() const {return Lambda_;}

  /// Gets value of **passes_**.
  auto Passes() const {return passes_;}

  /// Gets value of **output_every_n_passes_**.
  auto Output() const {return output_every_n_passes_;}

  /// Gets the total number of attempted moves.
  auto TotalMoves() const {return std::get<0>(attempted_moves_) +
                                  std::get<1>(attempted_moves_) +
                                  std::get<2>(attempted_moves_) +
                                  std::get<3>(attempted_moves_) +
                                  std::get<4>(attempted_moves_);}

  /// Gets attempted (2,3) moves.
  auto TwoThreeMoves() const {return std::get<0>(attempted_moves_);}

  /// Gets successful (2,3) moves.
  auto SuccessfulTwoThreeMoves() const {return std::get<0>(successful_moves_);}

  /// Gets attempted (3,2) moves.
  auto ThreeTwoMoves() const {return std::get<1>(attempted_moves_);}

  /// Gets successful (3,2) moves.
  auto SuccessfulThreeTwoMoves() const {return std::get<1>(successful_moves_);}

  /// Gets attempted (2,6) moves.
  auto TwoSixMoves() const {return std::get<2>(attempted_moves_);}

  /// Gets successful (2,6) moves.
  auto SuccessfulTwoSixMoves() const {return std::get<2>(successful_moves_);}

  /// Gets attempted (6,2) moves.
  auto SixTwoMoves() const {return std::get<3>(attempted_moves_);}

  /// Gets successful (6,2) moves.
  auto SuccessfulSixTwoMoves() const {return std::get<3>(attempted_moves_);}

  /// Gets attempted (4,4) moves.
  auto FourFourMoves() const {return std::get<4>(attempted_moves_);}

  /// Gets successful (4,4) moves.
  auto SuccessfulFourFourMoves() const {return std::get<4>(attempted_moves_);}

  /// Gets the vector of **Edge_tuples** corresponding to
  /// movable timelike edges.
  auto MovableTimelikeEdges() const {return edge_types_.first;}

  /// Gets the vector of **Cell_handles** corresponding to
  /// movable (3,1) simplices.
  auto MovableThreeOneSimplices() const {return std::get<0>(simplex_types_);}

  /// Gets the vector of **Cell_handles** corresponding to
  /// movable (2,2) simplices.
  auto MovableTwoTwoSimplices() const {return std::get<1>(simplex_types_);}

  /// Gets the vector of **Cell_handles** corresponding to
  /// movable (1,3) simplices.
  auto MovableOneThreeSimplices() const {return std::get<2>(simplex_types_);}

  /// Gets current number of timelike edges
  auto TimelikeEdges() const {return N1_TL_;}

  /// Gets current number of (3,1) and (1,3) simplices
  auto ThreeOneSimplices() const {return N3_31_;}

  /// Gets current number of (2,2) simplices
  auto TwoTwoSimplices() const {return N3_22_;}

  /// Gets current total number of simplices
  auto CurrentTotalSimplices() const {return N3_31_ + N3_22_;}

  /// Calculate the probability of making a move divided by the
  /// probability of its reverse, that is:
  /// \f[a_1=\frac{move[i]}{\sum\limits_{i}move[i]}\f]
  ///
  /// @param[in] move The type of move
  /// @returns \f$a_1=\frac{move[i]}{\sum\limits_{i}move[i]}\f$
  auto CalculateA1(move_type move) const {
    auto total_moves = this->TotalMoves();
    auto this_move = 5;
    auto move_name = "";
    switch (move) {
      case move_type::TWO_THREE:
        this_move = std::get<0>(attempted_moves_);
        move_name = "(2,3)";
        break;
      case move_type::THREE_TWO:
        this_move = std::get<1>(attempted_moves_);
        move_name = "(3,2)";
        break;
      case move_type::TWO_SIX:
        this_move = std::get<2>(attempted_moves_);
        move_name = "(2,6)";
        break;
      case move_type::SIX_TWO:
        this_move = std::get<3>(attempted_moves_);
        move_name = "(6,2)";
        break;
      case move_type::FOUR_FOUR:
        this_move = std::get<4>(attempted_moves_);
        move_name = "(4,4)";
        break;
    }
    // Set precision for initialization and assignment functions
    mpfr_set_default_prec(PRECISION);

    // Initialize for MPFR
    mpfr_t r1, r2, a1;
    mpfr_inits2(PRECISION, r1, r2, a1, nullptr);

    mpfr_init_set_ui(r1, this_move, MPFR_RNDD);     // r1 = this_move
    mpfr_init_set_ui(r2, total_moves, MPFR_RNDD);   // r2 = total_moves

    // The result
    mpfr_div(a1, r1, r2, MPFR_RNDD);                // a1 = r1/r2

    // std::cout << "A1 is " << mpfr_out_str(stdout, 10, 0, a1, MPFR_RNDD)

    // Convert mpfr_t total to Gmpzf result by using Gmpzf(double d)
    Gmpzf result = Gmpzf(mpfr_get_d(a1, MPFR_RNDD));
    // MP_Float result = MP_Float(mpfr_get_ld(a1, MPFR_RNDD));

    // Free memory
    mpfr_clears(r1, r2, a1, nullptr);

    #ifndef NDEBUG
    std::cout << "TotalMoves() = " << total_moves << std::endl;
    std::cout << move_name << " moves = " << this_move << std::endl;
    std::cout << "A1 is " << result << std::endl;
    #endif

    return result;
  }  // CalculateA1()

  /// Calculate \f$a_2=e^{\Delta S}\f$
  ///
  /// @param[in] move The type of move
  /// @returns \f$a_2=e^{\Delta S}\f$
  auto CalculateA2(move_type move) const {
    auto currentS3Action = S3_bulk_action(N1_TL_,
                                          N3_31_,
                                          N3_22_,
                                          Alpha_,
                                          K_,
                                          Lambda_);
    auto newS3Action = static_cast<Gmpzf>(0);
    // auto newS3Action = static_cast<MP_Float>(0);
    switch (move) {
      case move_type::TWO_THREE:
        // A (2,3) move removes a timelike edge and
        // adds a (2,2) simplex
        newS3Action = S3_bulk_action(N1_TL_-1,
                                     N3_31_,
                                     N3_22_+1,
                                     Alpha_,
                                     K_,
                                     Lambda_);
        break;
      case move_type::THREE_TWO:
        // A (3,2) move adds a timelike edge and
        // removes a (2,2) simplex
        newS3Action = S3_bulk_action(N1_TL_+1,
                                   N3_31_,
                                   N3_22_-1,
                                   Alpha_,
                                   K_,
                                   Lambda_);
        break;
      case move_type::TWO_SIX:
        // A (2,6) move adds 2 timelike edges and
        // adds 2 (1,3) and 2 (3,1) simplices
        newS3Action = S3_bulk_action(N1_TL_+2,
                                   N3_31_+4,
                                   N3_22_,
                                   Alpha_,
                                   K_,
                                   Lambda_);
        break;
      case move_type::SIX_TWO:
        // A (6,2) move removes 2 timelike edges and
        // removes 2 (1,3) and 2 (3,1) simplices
        newS3Action = S3_bulk_action(N1_TL_-2,
                                   N3_31_,
                                   N3_22_-4,
                                   Alpha_,
                                   K_,
                                   Lambda_);
        break;
      case move_type::FOUR_FOUR:
        // A (4,4) move changes nothing, and e^0==1
        #ifndef NDEBUG
        std::cout << "A2 is 1" << std::endl;
        #endif
        return static_cast<Gmpzf>(1);
    }

    auto exponent = newS3Action - currentS3Action;
    auto exponent_double = Gmpzf_to_double(exponent);

    // if exponent > 0 then e^exponent >=1 so according to Metropolis
    // algorithm return A2=1
    if (exponent >=0) return static_cast<Gmpzf>(1);

    // Set precision for initialization and assignment functions
    mpfr_set_default_prec(PRECISION);

    // Initialize for MPFR
    mpfr_t r1, a2;
    mpfr_inits2(PRECISION, r1, a2, nullptr);

    // Set input parameters and constants to mpfr_t equivalents
    mpfr_init_set_d(r1, exponent_double, MPFR_RNDD);   // r1 = exponent

    // e^exponent
    mpfr_exp(a2, r1, MPFR_RNDD);

    // Convert mpfr_t total to Gmpzf result by using Gmpzf(double d)
    Gmpzf result = Gmpzf(mpfr_get_d(a2, MPFR_RNDD));

    // Free memory
    mpfr_clears(r1, a2, nullptr);

    #ifndef NDEBUG
    std::cout << "A2 is " << result << std::endl;
    #endif

    return result;
  }  // CAlculateA2()

  void attempt_move(const move_type move) noexcept {
    // Calculate probability
    auto a1 = CalculateA1(move);
    // Make move if random number < probability
    auto a2 = CalculateA2(move);

    // Cast move_type to corresponding size_t
    // const auto move_choice = 0;
    const auto move_choice = static_cast<size_t>(move);

    const auto trialval = generate_probability();
    const auto trial = Gmpzf(static_cast<double>(trialval));

    #ifndef NDEBUG
    std::cout << "trialval = " << trialval << std::endl;
    std::cout << "trial = " << trial << std::endl;
    #endif

    if (trial <= a1*a2) {
      // Move accepted

      switch (move_choice) {
        case static_cast<int>(move_type::TWO_THREE):
          std::cout << "(2,3) move" << std::endl;
          make_23_move(universe_ptr_, simplex_types_, attempted_moves_);
          // Increment N3_22_, N1_TL_ and successful_moves_
          ++N3_22_;
          ++N1_TL_;
          ++std::get<0>(successful_moves_);
          break;
        case static_cast<int>(move_type::THREE_TWO):
          std::cout << "(3,2) move" << std::endl;
          make_32_move(universe_ptr_, edge_types_, attempted_moves_);
          // Decrement N3_22_ and N1_TL_, increment successful_moves_
          --N3_22_;
          --N1_TL_;
          ++std::get<1>(successful_moves_);
          break;
        case static_cast<int>(move_type::TWO_SIX):
          std::cout << "(2,6) move" << std::endl;
          make_26_move(universe_ptr_, simplex_types_, attempted_moves_);
          // Increment N3_31, N1_TL_ and successful_moves_
          N3_31_ += 4;
          N1_TL_ += 2;
          // We don't currently keep track of changes to spacelike edges
          // because it doesn't figure in the bulk action formula, but if
          // we did there would be 3 additional spacelike edges to add here.
          ++std::get<2>(successful_moves_);
          break;
        case static_cast<int>(move_type::SIX_TWO):
          std::cout << "(6,2) move" << std::endl;
          break;
        case static_cast<int>(move_type::FOUR_FOUR):
          std::cout << "(4,4) move" << std::endl;
          break;
      }
    } else {
      // Move rejected
      // Increment attempted_moves_
      // ++std::get<move_choice>(attempted_moves);
      switch (move_choice) {
        case static_cast<int>(move_type::TWO_THREE):
          ++std::get<0>(attempted_moves_);
          break;
        case static_cast<int>(move_type::THREE_TWO):
          ++std::get<1>(attempted_moves_);
          break;
        case static_cast<int>(move_type::TWO_SIX):
          ++std::get<2>(attempted_moves_);
          break;
        case static_cast<int>(move_type::SIX_TWO):
          ++std::get<3>(attempted_moves_);
          break;
        case static_cast<int>(move_type::FOUR_FOUR):
          ++std::get<4>(attempted_moves_);
          break;
      }
    }

    #ifndef NDEBUG
    std::cout << "Attempting move." << std::endl;
    std::cout << "Move type = " << static_cast<unsigned>(move_choice)
              << std::endl;
    std::cout << "Trial = " << trial << std::endl;
    std::cout << "A1 = " << a1 << std::endl;
    std::cout << "A2 = " << a2 << std::endl;
    std::cout << "A1*A2 = " << a1*a2 << std::endl;
    std::cout << ((trial <= a1*a2) ? "Move accepted."
                                   : "Move rejected.") << std::endl;
    std::cout << "Successful (2,3) moves = " << SuccessfulTwoThreeMoves()
              << std::endl;
    std::cout << "Attempted (2,3) moves = " << TwoThreeMoves() << std::endl;

    std::cout << "Successful (3,2) moves = " << SuccessfulThreeTwoMoves()
              << std::endl;
    std::cout << "Attempted (3,2) moves = " << ThreeTwoMoves() << std::endl;

    std::cout << "Successful (2,6) moves = " << SuccessfulTwoSixMoves()
              << std::endl;
    std::cout << "Attempted (2,6) moves = " << TwoSixMoves() << std::endl;

    std::cout << "Successful (6,2) moves = " << SuccessfulSixTwoMoves()
              << std::endl;
    std::cout << "Attempted (6,2) moves = " << SixTwoMoves() << std::endl;

    std::cout << "Successful (4,4) moves = " << SuccessfulFourFourMoves()
              << std::endl;
    std::cout << "Attempted (4,4) moves = " << FourFourMoves() << std::endl;
    #endif
  }  // attempt_move()

  /// @brief () operator
  ///
  /// This makes the Metropolis class into a functor. Minimal setup of
  /// runtime job parameters is handled by the constructor. This () operator
  /// conducts all of algorithmic work for Metropolis-Hastings on the
  /// Delaunay triangulation.
  ///
  /// @param[in] universe_ptr A std::unique_ptr to the Delaunay triangulation,
  /// which should already be initialized with **make_triangulation()**.
  /// @returns The std::unique_ptr to the Delaunay triangulation. Note that
  /// this sets the data member **universe_ptr_** to null, and so no operations
  /// can be successfully carried out on **universe_ptr_** when operator()
  /// returns. Instead, they should be conducted on the results of this
  /// function call.
  template <typename T>
  auto operator()(T&& universe_ptr) -> decltype(universe_ptr) {
    #ifndef NDEBUG
    std::cout << "operator() called." << std::endl;
    #endif
    std::cout << "Starting Metropolis-Hastings algorithm ..." << std::endl;
    // Populate member data
    universe_ptr_ = std::move(universe_ptr);
    simplex_types_ = classify_simplices(universe_ptr_);
    edge_types_ = classify_edges(universe_ptr_);
    N3_31_ = static_cast<uintmax_t>(std::get<0>(simplex_types_).size() +
                                            std::get<2>(simplex_types_).size());
    std::cout << "N3_31_ = " << N3_31_ << std::endl;

    N3_22_ = static_cast<uintmax_t>(std::get<1>(simplex_types_).size());
    std::cout << "N3_22_ = " << N3_22_ << std::endl;

    N1_TL_ = static_cast<uintmax_t>(edge_types_.first.size());
    std::cout << "N1_TL_ = " << N1_TL_ << std::endl;

    // Attempt each type of move to populate **attempted_moves_**
    universe_ptr_ = std::move(make_23_move(universe_ptr_,
                                           simplex_types_, attempted_moves_));
    // A (2,3) move increases (2,2) simplices and timelike edges by 1
    ++N3_22_;
    ++N1_TL_;
    ++std::get<0>(successful_moves_);

    universe_ptr_ = std::move(make_32_move(universe_ptr_,
                                           edge_types_, attempted_moves_));
    // A (3,2) move decreases (2,2) simplices and timelike edges by 1
    --N3_22_;
    --N1_TL_;
    ++std::get<1>(successful_moves_);

    universe_ptr_ = std::move(make_26_move(universe_ptr_,
                                           simplex_types_, attempted_moves_));
    // A (2,6) move increases (1,3) and (3,1) simplices by 4
    // and timelike edges by 2
    N3_31_ += 4;
    N1_TL_ += 2;
    ++std::get<2>(successful_moves_);

    // Other moves go here ...

    auto total_simplices_this_pass = 10;  // CurrentTotalSimplices();
    for (auto move_attempt = 0; move_attempt < total_simplices_this_pass;
         ++move_attempt) {
      // Pick a move to attempt
      auto move_choice = generate_random_unsigned(0, 2);
      #ifndef NDEBUG
      std::cout << "Move choice = " << move_choice << std::endl;
      #endif

      // Convert unsigned to move_choice enum
      auto move = static_cast<move_type>(move_choice);
      attempt_move(move);
    }

    return universe_ptr_;
  }

 private:
  Delaunay universe;
  ///< The type of triangulation.
  std::unique_ptr<decltype(universe)>
    universe_ptr_ = std::make_unique<decltype(universe)>(universe);
  ///< A std::unique_ptr to the Delaunay triangulation. For this reason you
  /// should not access this member directly, as operator() may be called
  /// at any time and null it out via std::move().
  long double Alpha_;
  ///< Alpha is the length of timelike edges.
  long double K_;
  ///< \f$K=\frac{1}{8\pi G_{N}}\f$
  long double Lambda_;
  ///< \f$\lambda=\frac{\Lambda}{8\pi G_{N}}\f$ where \f$\Lambda\f$ is
  /// the cosmological constant.
  uintmax_t N1_TL_ {0};
  ///< The current number of timelike edges, some of which may not be movable.
  uintmax_t N3_31_ {0};
  ///< The current number of (3,1) and (1,3) simplices, some of which may not
  /// be movable.
  uintmax_t N3_22_ {0};
  ///< The current number of (2,2) simplices, some of which may not be movable.
  unsigned passes_;  ///< Number of passes of ergodic moves on triangulation.
  unsigned output_every_n_passes_;  ///< How often to print/write output.
  move_tuple attempted_moves_ {0, 0, 0, 0, 0};
  ///< Attempted (2,3), (3,2), (2,6), (6,2), and (4,4) moves.
  move_tuple successful_moves_ {0, 0, 0, 0, 0};
  ///< Successful (2,3), (3,2), (2,6), (6,2), and (4,4) moves.
  std::tuple<std::vector<Cell_handle>,
             std::vector<Cell_handle>,
             std::vector<Cell_handle>> simplex_types_;
  ///< Movable (3,1), (2,2) and (1,3) simplices.
  std::pair<std::vector<Edge_tuple>, unsigned> edge_types_;
  ///< Movable timelike and spacelike edges.
};  // Metropolis



/// @brief Apply the Metropolis-Hastings algorithm
///
/// The Metropolis-Hastings algorithm is a Markov Chain Monte Carlo method.
/// The probability of making an ergodic (Pachner) move is:
///
/// @param[in] universe_ptr A std::unique_ptr to the Delaunay triangulation
/// @param[in] number_of_passes The number of passes made with MCMC, where a
/// pass is defined as a number of attempted moves equal to the current number
/// of simplices.
/// @param[in] output_every_n_passes Prints/saves to file the current
/// Delaunay triangulation every n passes.
/// @returns universe_ptr A std::unique_ptr to the Delaunay triangulation after
/// the move has been made
// template <typename T>
// auto metropolis(T&& universe_ptr, unsigned number_of_passes,
//                 unsigned output_every_n_passes) noexcept
//                 -> decltype(universe_ptr) {
//   std::cout << "Starting ..." << std::endl;
//
//   auto simplex_types = classify_simplices(universe_ptr);
//   auto edge_types = edge_types = classify_edges(universe_ptr);
//
//
//   auto attempted_moves_per_pass = universe_ptr->number_of_finite_cells();
//   // First, attempt a move of each type
//   // attempt_23_move();
//   ++std::get<0>(attempted_moves);
//   // attempt_32_move();
//   ++std::get<1>(attempted_moves);
//   // attempt_26_move();
//   ++std::get<2>(attempted_moves);
//   while (std::get<0>(attempted_moves) +
//          std::get<1>(attempted_moves) +
//          std::get<2>(attempted_moves) < attempted_moves_per_pass) {
//     // Fix this function to use attempt[i]/total attempts
//     auto move = generate_random_unsigned(1, 3);
//     std::cout << "Move #" << move << std::endl;
//
//     switch (move) {
//       case (move_type::TWO_THREE):
//         std::cout << "Move 1 (2,3) picked" << std::endl;
//         // attempt_23_move()
//         ++std::get<0>(attempted_moves);
//         break;
//       case (move_type::THREE_TWO):
//         std::cout << "Move 2 (3,2) picked" << std::endl;
//         // attempt_32_move()
//         ++std::get<1>(attempted_moves);
//         break;
//       case (move_type::TWO_SIX):
//         std::cout << "Move 3 (2,6) picked" << std::endl;
//         // attempt_26_move()
//         ++std::get<2>(attempted_moves);
//         break;
//       default:
//         std::cout << "Oops!" << std::endl;
//         break;
//     }
//   }
//   return universe_ptr;
// }  // metropolis()
// auto metropolis =
//   std::make_unique<decltype(universe)>(Metropolis(universe));
// std::unique_ptr<Metropolis> metropolis = std::make_unique<Metropolis>(universe);
// Metropolis metropolis(universe);

// // Main loop of program
// for (auto i = 0; i < passes; ++i) {
//   // Initialize data and data structures needed for ergodic moves
//   // each pass.
//   // make_23_move(&SphericalUniverse, &two_two) does the (2,3) move
//   // two_two is populated via classify_3_simplices()
//
//   // Get timelike edges V2 for make_32_move(&SphericalUniverse, &V2)
//   std::vector<Edge_tuple> V2;
//   auto N1_SL = static_cast<unsigned>(0);
//   get_timelike_edges(SphericalUniverse, &V2, &N1_SL);
//
//   auto moves_this_pass = SphericalUniverse.number_of_finite_cells();
//
//   std::cout << "Pass #" << i+1 << " is "
//             << moves_this_pass
//             << " attempted moves." << std::endl;
//
//   for (auto j = 0; j < moves_this_pass; ++j) {
//     // Metropolis algorithm to select moves goes here
//   }
// }

#endif  // SRC_METROPOLIS_H_
