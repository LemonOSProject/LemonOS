#pragma once

#include <string>
#include <cassert>

#include <lemon/gui/variant.h>

namespace Lemon::GUI{
	class DataModel{
	public:
		enum Sort {
			SortAscending,
			SortDescending,
		};

		class Column final {
		public:
			Column(const std::string& name, bool isEditable = false) : name(name), isEditable(isEditable){

			}

			inline const std::string& Name() const { return name; }

			inline bool IsEditable() const { return isEditable; }

		private:
			std::string name; // Column name/label

			bool isEditable = false; // Is the data in the column editable?
		};

		virtual ~DataModel() = default;

		/////////////////////////////
		/// \brief Get column count
		///
		/// Get number of data columns
		///
		/// \return Number of data columns as an integer
		/////////////////////////////
		virtual int ColumnCount() const = 0;

		/////////////////////////////
		/// \brief Get row count
		///
		/// Get number of data entries/rows
		///
		/// \return Number of data rows as an integer
		/////////////////////////////
		virtual int RowCount() const = 0;

		/////////////////////////////
		/// \brief Get column name
		///
		/// \return Column name
		/////////////////////////////
		virtual const std::string& ColumnName(int column) const = 0;

		/////////////////////////////
		/// \brief Sort by specified column
		///
		/// \param column Index of column to sort by
		/////////////////////////////
		virtual void Sort([[maybe_unused]] int column) {};

		/////////////////////////////
		/// \brief Get data entry
		///
		/// \param row Row/entry to retrieve
		/// \param column Column/value to retrieve
		/////////////////////////////
		virtual Variant GetData(int row, int column) = 0;

		/////////////////////////////
		/// \brief Get size hint
		///
		/// Retrieves the recommended display width for the column
		///
		/// \param row Row/entry to retrieve
		/// \param column Column/value to retrieve
		/////////////////////////////
		virtual int SizeHint(int column) = 0;

		virtual void Refresh() = 0;
	private:

	};
}