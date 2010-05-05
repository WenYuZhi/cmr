//          Copyright Matthias Walter 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef VIOLATOR_SEARCH_HPP_
#define VIOLATOR_SEARCH_HPP_

#include <boost/numeric/ublas/matrix_proxy.hpp>

#include "algorithm.hpp"
#include "matroid.hpp"
#include "signing.hpp"
#include "logger.hpp"

namespace tu {
  namespace detail {

    inline matroid_element_set find_smallest_irregular_minor (const decomposed_matroid* decomposition, bool collect_extra_elements = true)
    {
      if (decomposition->is_leaf ())
      {
        const decomposed_matroid_leaf* leaf = (decomposed_matroid_leaf*) decomposition;

        if (leaf->is_regular ())
          return matroid_element_set ();

        matroid_element_set result;
        std::copy (leaf->elements ().begin (), leaf->elements ().end (), std::inserter (result, result.end ()));
        if (collect_extra_elements)
          std::copy (leaf->extra_elements ().begin (), leaf->extra_elements ().end (), std::inserter (result, result.end ()));
        return result;
      }
      else
      {
        const decomposed_matroid_separator* separator = (decomposed_matroid_separator*) decomposition;

        matroid_element_set first_elements = find_smallest_irregular_minor (separator->first (), collect_extra_elements);
        matroid_element_set second_elements = find_smallest_irregular_minor (separator->second (), collect_extra_elements);
        if (first_elements.empty ())
          return second_elements;
        else if (second_elements.empty ())
          return first_elements;
        else
          return (first_elements.size () < second_elements.size ()) ? first_elements : second_elements;
      }
    }

    template <typename InputIterator, typename OutputIterator1, typename OutputIterator2>
    std::pair <OutputIterator1, OutputIterator2> split_elements (InputIterator first, InputIterator beyond, OutputIterator1 rows,
        OutputIterator2 columns)
    {
      for (; first != beyond; ++first)
      {
        if (*first > 0)
          *columns++ = *first;
        else
          *rows++ = *first;
      }
      return std::make_pair (rows, columns);
    }

    template <typename MatrixType>
    void create_indirect_matroid (const MatrixType& input_matrix, const matroid_element_set& row_elements,
        const matroid_element_set& column_elements, integer_matroid& sub_matroid, submatrix_indices& sub_indices)
    {
      sub_matroid.resize (row_elements.size (), column_elements.size ());
      submatrix_indices::vector_type row_vector (row_elements.size ());
      submatrix_indices::vector_type column_vector (column_elements.size ());

      size_t index = row_elements.size () - 1;
      for (matroid_element_set::const_iterator iter = row_elements.begin (); iter != row_elements.end (); ++iter)
      {
        sub_matroid.name1 (index) = *iter;
        row_vector[index] = -1 - *iter;
        --index;
      }

      index = 0;
      for (matroid_element_set::const_iterator iter = column_elements.begin (); iter != column_elements.end (); ++iter)
      {
        sub_matroid.name2 (index) = *iter;
        column_vector[index] = -1 + *iter;
        ++index;
      }

      sub_indices.rows = submatrix_indices::indirect_array_type (row_vector.size (), row_vector);
      sub_indices.columns = submatrix_indices::indirect_array_type (column_vector.size (), column_vector);

      //      std::cout << "create submatrix [";
      //      std::copy (sub_indices.rows.begin (), sub_indices.rows.end (), std::ostream_iterator <int> (std::cout, " "));
      //      std::cout << "] [";
      //      std::copy (sub_indices.columns.begin (), sub_indices.columns.end (), std::ostream_iterator <int> (std::cout, " "));
      //      matrix_print (input_matrix);
      //      std::cout << "]" << std::endl;
    }

    class violator_strategy
    {
    public:
      violator_strategy (const integer_matrix& input_matrix, const matroid_element_set& row_elements, const matroid_element_set& column_elements,
          logger& log) :
        _input_matrix (input_matrix), _row_elements (row_elements), _column_elements (column_elements), _log (log)
      {
        if (log.is_updating () || _log.is_verbose ())
        {
          std::cout << "\nMatrix is NOT totally unimodular. Searching the violating submatrix...\n" << std::endl;
        }
      }

      virtual void search () = 0;

      inline void create_matrix (submatrix_indices& indices) const
      {
        integer_matroid sub_matroid;
        create_indirect_matroid (_input_matrix, _row_elements, _column_elements, sub_matroid, indices);

        //        std::cout << "matrix created" << std::endl;
      }

    protected:

      virtual void shrink (const matroid_element_set& row_elements, const matroid_element_set& column_elements)
      {
        //        std::cout << "shrinking to " << row_elements.size () << " x " << column_elements.size () << std::endl;

#ifndef NDEBUG
        typedef boost::numeric::ublas::matrix_indirect <const integer_matrix, submatrix_indices::indirect_array_type> indirect_matrix_t;

        integer_matroid matroid;
        submatrix_indices sub_indices;

        create_indirect_matroid (_input_matrix, row_elements, column_elements, matroid, sub_indices);
        indirect_matrix_t sub_matrix (_input_matrix, sub_indices.rows, sub_indices.columns);

        if (is_totally_unimodular (sub_matrix))
        {
          std::cout << "submatrix is t.u., but should not:" << std::endl;
          matrix_print (sub_matrix);

          assert (false);
        }
#endif

        _row_elements = row_elements;
        _column_elements = column_elements;

      }

      inline bool test (const matroid_element_set& row_elements, const matroid_element_set& column_elements)
      {
        //        std::cout << "[[[TESTING " << row_elements.size () << " x " << column_elements.size () << " SUBMATRIX]]]" << std::endl;

        typedef boost::numeric::ublas::matrix_indirect <const integer_matrix, submatrix_indices::indirect_array_type> indirect_matrix_t;

        integer_matroid matroid;
        submatrix_indices sub_indices;

        create_indirect_matroid (_input_matrix, row_elements, column_elements, matroid, sub_indices);
        indirect_matrix_t sub_matrix (_input_matrix, sub_indices.rows, sub_indices.columns);

        /// Signing test

        decomposed_matroid* decomposition;
        integer_matrix matrix (sub_matrix);

        //        std::cout << "Copy to work on:\n" << std::flush;
        //        matroid_print (matroid, matrix);

        //        {
        //          bool gh_is_tu;
        //          {
        //            integer_matrix copy = matrix;
        //            std::cout << (sign_matrix (copy) ? "can be signed to original" : "cannot be signed to original. gh test is NO indicator now!")
        //                << std::endl;
        //            gh_is_tu = ghouila_houri_is_totally_unimodular (copy);
        //            std::cout << "ghouila-houri said: " << (gh_is_tu ? "matrix is TU" : "matrix is NOT TU") << std::endl;
        //          }
        //        }

        if (!is_signed_matrix (matrix))
        {
          if (_log.is_updating () || _log.is_verbose ())
          {
            std::cout << "Submatrix did not pass the signing test. It is NOT totally unimodular.\n" << std::endl;
          }
          shrink (row_elements, column_elements);
          return false;
        }

        /// Remove sign from matrix
        support_matrix (matrix);

        /// Matroid decomposition
        bool is_tu;
        boost::tie (is_tu, decomposition) = decompose_binary_matroid (matroid, matrix, matroid_element_set (), true, _log);

        //        assert (is_tu == gh_is_tu);
        //        
        //        std::cout << "decompose returned " << ((int) (is_tu)) << std::endl;
        //        matrix_print (matrix);
        //        if (!is_tu && gh_is_tu)
        //        {
        //          std::cout << "TU matrix recognized as non-TU!!!" << std::endl;
        //          assert (false);
        //        }
        //        else if (is_tu && !gh_is_tu)
        //        {
        //          std::cout << "non-TU matrix recognized as TU!!!" << std::endl;
        //          assert (false);
        //        }

        if (is_tu)
        {
          if (_log.is_updating () || _log.is_verbose ())
          {
            std::cout << "\nSubmatrix is totally unimodular.\n" << std::endl;
          }
          delete decomposition;
          return true;
        }

        matroid_element_set rows, columns, elements = detail::find_smallest_irregular_minor (decomposition);
        delete decomposition;

        //        std::cout << "found smallest irregular minor." << std::endl;

        detail::split_elements (elements.begin (), elements.end (), std::inserter (rows, rows.end ()), std::inserter (columns, columns.end ()));

        //                std::cout << "calling shrink" << std::endl;

        //        std::cout << "ordinary: " << row_elements.size () << " x " << column_elements.size () << std::endl;
        //        std::cout << "decomposition-based: " << rows.size () << " x " << columns.size () << std::endl;

        //        assert (row_elements.size() == rows.size());
        //        assert (column_elements.size() == columns.size());

        if (_log.is_updating () || _log.is_verbose ())
        {
          if (rows.size () < row_elements.size () || columns.size () < column_elements.size ())
          {
            std::cout << "\nSubmatrix is NOT totally unimodular. A " << rows.size () << " x " << columns.size ()
                << " non-totally unimodular submatrix was identified, too.\n" << std::endl;
          }
          else
          {
            std::cout << "\nSubmatrix is NOT totally unimodular.\n" << std::endl;
          }
        }

        //        shrink (row_elements, column_elements);
        shrink (rows, columns);

        //        std::cout << "shrink done." << std::endl;

        return false;
      }

      inline bool test_forbidden (const matroid_element_set& forbidden_elements)
      {
        /// Setup rows and columns
        matroid_element_set rows, columns;
        for (matroid_element_set::const_iterator iter = _row_elements.begin (); iter != _row_elements.end (); ++iter)
        {
          if (forbidden_elements.find (*iter) == forbidden_elements.end ())
          {
            rows.insert (*iter);
          }
        }
        for (matroid_element_set::const_iterator iter = _column_elements.begin (); iter != _column_elements.end (); ++iter)
        {
          if (forbidden_elements.find (*iter) == forbidden_elements.end ())
          {
            columns.insert (*iter);
          }
        }

        return test (rows, columns);
      }

    protected:
      const integer_matrix& _input_matrix;
      matroid_element_set _row_elements;
      matroid_element_set _column_elements;
      logger& _log;
    };

    class single_violator_strategy: public violator_strategy
    {
    public:
      single_violator_strategy (const integer_matrix& input_matrix, const matroid_element_set& row_elements,
          const matroid_element_set& column_elements, logger& log) :
        violator_strategy (input_matrix, row_elements, column_elements, log)
      {

      }

      virtual void search ()
      {
        std::vector <int> all_elements;
        std::copy (_row_elements.begin (), _row_elements.end (), std::back_inserter (all_elements));
        std::copy (_column_elements.begin (), _column_elements.end (), std::back_inserter (all_elements));

        for (std::vector <int>::const_iterator iter = all_elements.begin (); iter != all_elements.end (); ++iter)
        {
          //          std::cout << "\nSearching for violating submatrix in " << _row_elements.size () << " x " << _column_elements.size () << " matrix."
          //              << std::endl;

          //          std::cout << "\n\nTrying to remove " << *iter << " from current matrix:" << std::endl;
          //          {
          //            typedef boost::numeric::ublas::matrix_indirect <const integer_matrix, submatrix_indices::indirect_array_type> indirect_matrix_t;
          //
          //            integer_matroid sub_matroid;
          //            submatrix_indices sub_indices;
          //
          //            create_indirect_matroid (_input_matrix, _row_elements, _column_elements, sub_matroid, sub_indices);
          //            indirect_matrix_t sub_matrix (_input_matrix, sub_indices.rows, sub_indices.columns);
          //
          //            matroid_print (sub_matroid, sub_matrix);
          //          }

          if (_row_elements.find (*iter) == _row_elements.end () && _column_elements.find (*iter) == _column_elements.end ())
            continue;

          matroid_element_set rows (_row_elements);
          matroid_element_set columns (_column_elements);
          rows.erase (*iter);
          columns.erase (*iter);
          //          bool result = 
          test (rows, columns);

          //          std::cout << "test result =  " << result << std::endl;
        }

        //        std::cout << "search done." << std::endl;
      }
    };

    class greedy_violator_strategy: public violator_strategy
    {
    public:
      greedy_violator_strategy (const integer_matrix& input_matrix, const matroid_element_set& row_elements,
          const matroid_element_set& column_elements, logger& log) :
        violator_strategy (input_matrix, row_elements, column_elements, log)
      {

      }

      /**
       * Tests minors given in a vector of sets.
       * 
       * @param bundles Vector of Sets containing the removed elements.
       * @return true iff a test failed, i.e. the submatrix was not totally unimodular.
       */

      bool test_bundles (const std::vector <matroid_element_set>& bundles, bool abort_on_shrink)
      {
        bool success = false;
        for (std::vector <matroid_element_set>::const_iterator bundle_iter = bundles.begin (); bundle_iter != bundles.end (); ++bundle_iter)
        {
          //          std::cout << "size = " << _row_elements.size () << " x " << _column_elements.size () << std::endl;
          if (!test_forbidden (*bundle_iter))
          {
            if (abort_on_shrink)
              return true;
            success = true;
          }
        }

        return success;
      }

      virtual void search ()
      {
        for (float rate = 0.8f; rate > 0.02f; rate *= 0.5f)
        {
          size_t row_amount, column_amount;
          bool abort_on_shrink;
          if (rate > 0.04f)
          {
            row_amount = int(_row_elements.size () * rate);
            column_amount = int(_column_elements.size () * rate);
            abort_on_shrink = true;
            if (row_amount == 0 || column_amount == 0)
            {
              row_amount = 1;
              column_amount = 1;
              abort_on_shrink = false;
              rate = 0.0f;
            }
          }
          else
          {
            row_amount = 1;
            column_amount = 1;
            abort_on_shrink = false;
          }

          //          std::cout << "amounts = " << row_amount << ", " << column_amount << std::endl;

          typedef std::vector <matroid_element_set::value_type> matroid_element_vector;
          matroid_element_vector shuffled_rows, shuffled_columns;
          std::copy (_row_elements.begin (), _row_elements.end (), std::back_inserter (shuffled_rows));
          std::copy (_column_elements.begin (), _column_elements.end (), std::back_inserter (shuffled_columns));

          std::random_shuffle (shuffled_rows.begin (), shuffled_rows.end ());
          std::random_shuffle (shuffled_columns.begin (), shuffled_columns.end ());

          std::vector <matroid_element_set> bundles;

          for (matroid_element_vector::const_iterator iter = shuffled_rows.begin (); iter + row_amount <= shuffled_rows.end (); iter += row_amount)
          {
            bundles.push_back (matroid_element_set ());
            std::copy (iter, iter + row_amount, std::inserter (bundles.back (), bundles.back ().end ()));
          }

          for (matroid_element_vector::const_iterator iter = shuffled_columns.begin (); iter + column_amount <= shuffled_columns.end (); iter
              += column_amount)
          {
            bundles.push_back (matroid_element_set ());
            std::copy (iter, iter + column_amount, std::inserter (bundles.back (), bundles.back ().end ()));
          }

          //          std::cout << "Having " << bundles.size () << " bundles at rate " << rate << std::endl;

          if (test_bundles (bundles, abort_on_shrink))
          {
            rate *= 2.0f;
          }
        }
      }
    };

  }
}

#endif /* VIOLATOR_SEARCH_HPP_ */
