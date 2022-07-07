////////////////////////////////////////////////////////////////////////////////
// maxcalorie.hh
//
// Compute the set of foods that maximizes the calories in foods, within
// a given maximum weight with the greedy method or exhaustive search.
//
///////////////////////////////////////////////////////////////////////////////


#pragma once


#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

// One food item available for purchase.
class FoodItem
{
	//
	public:

		//
		FoodItem
		(
			const std::string& description,
			double weight_ounces,
			double calories
		)
			:
			_description(description),
			_weight_ounces(weight_ounces),
			_calories(calories)
		{
			assert(!description.empty());
			assert(weight_ounces > 0);
		}

		//
		const std::string& description() const { return _description; }
		double weight() const { return _weight_ounces; }
		double foodCalories() const { return _calories; }

	//
	private:

		// Human-readable description of the food, e.g. "spicy chicken breast". Must be non-empty.
		std::string _description;

		// Food weight, in ounces; Must be positive
		double _weight_ounces;

		// Calories; most be non-negative.
		double _calories;
};


// Alias for a vector of shared pointers to FoodItem objects.
typedef std::vector<std::shared_ptr<FoodItem>> FoodVector;


// Load all the valid food items from the CSV database
// Food items that are missing fields, or have invalid values, are skipped.
// Returns nullptr on I/O error.
std::unique_ptr<FoodVector> load_food_database(const std::string& path)
{
	std::unique_ptr<FoodVector> failure(nullptr);

	std::ifstream f(path);
	if (!f)
	{
		std::cout << "Failed to load food database; Cannot open file: " << path << std::endl;
		return failure;
	}

	std::unique_ptr<FoodVector> result(new FoodVector);

	size_t line_number = 0;
	for (std::string line; std::getline(f, line); )
	{
		line_number++;

		// First line is a header row
		if ( line_number == 1 )
		{
			continue;
		}

		std::vector<std::string> fields;
		std::stringstream ss(line);

		for (std::string field; std::getline(ss, field, '^'); )
		{
			fields.push_back(field);
		}

		if (fields.size() != 3)
		{
			std::cout
				<< "Failed to load food database: Invalid field count at line " << line_number << "; Want 3 but got " << fields.size() << std::endl
				<< "Line: " << line << std::endl
				;
			return failure;
		}

		std::string
			descr_field = fields[0],
			weight_ounces_field = fields[1],
			calories_field = fields[2]
			;

		auto parse_dbl = [](const std::string& field, double& output)
		{
			std::stringstream ss(field);
			if ( ! ss )
			{
				return false;
			}

			ss >> output;

			return true;
		};

		std::string description(descr_field);
		double weight_ounces, calories;
		if (
			parse_dbl(weight_ounces_field, weight_ounces)
			&& parse_dbl(calories_field, calories)
		)
		{
			result->push_back(
				std::shared_ptr<FoodItem>(
					new FoodItem(
						description,
						weight_ounces,
						calories
					)
				)
			);
		}
	}

	f.close();

	return result;
}


// Convenience function to compute the total weight and calories in
// a FoodVector.
// Provide the FoodVector as the first argument
// The next two arguments will return the weight and calories back to
// the caller.
void sum_food_vector
(
	const FoodVector& foods,
	double& total_weight,
	double& total_calories
)
{
	total_weight = total_calories = 0;
	for (auto& food : foods)
	{
		total_weight += food->weight();
		total_calories += food->foodCalories();
	}
}


// Convenience function to print out each FoodItem in a FoodVector,
// followed by the total weight and calories of it.
void print_food_vector(const FoodVector& foods)
{
	std::cout << "*** food Vector ***" << std::endl;

	if ( foods.size() == 0 )
	{
		std::cout << "[empty food list]" << std::endl;
	}
	else
	{
		for (auto& food : foods)
		{
			std::cout
				<< "Ye olde " << food->description()
				<< " ==> "
				<< "Weight of " << food->weight() << " ounces"
				<< "; calories = " << food->foodCalories()
				<< std::endl
				;
		}

		double total_weight, total_calories;
		sum_food_vector(foods, total_weight, total_calories);
		std::cout
			<< "> Grand total weight: " << total_weight << " ounces" << std::endl
			<< "> Grand total calories: " << total_calories
			<< std::endl
			;
	}
}


// Filter the vector source, i.e. create and return a new FoodVector
// containing the subset of the food items in source that match given
// criteria.
// This is intended to:
//	1) filter out food with zero or negative calories that are irrelevant to // our optimization
//	2) limit the size of inputs to the exhaustive search algorithm since it // will probably be slow.
//
// Each food item that is included must have at minimum min_calories and
// at most max_calories.
//	(i.e., each included food item's calories must be between min_calories
// and max_calories (inclusive).
//
// In addition, the the vector includes only the first total_size food items
// that match these criteria.
std::unique_ptr<FoodVector> filter_food_vector
(
	const FoodVector& source,
	double min_calories,
	double max_calories,
	int total_size
)
{
	// Initialize the vector that will hold the filtered items.
	std::unique_ptr<FoodVector> FilteredFoodVector(new FoodVector);

	// Loop to go through all the items in the food vector.
	for(auto& food: source)
	{
		// If the item satisfies the requirements of the minimum calories, maximum
		// calories and within the size constraints, it will be added to our filtered
		// vector.
		if (food->foodCalories() >= min_calories && food->foodCalories() <= max_calories)
		{
			if (int(FilteredFoodVector->size()) < total_size)
			{
				FilteredFoodVector->push_back(food);
			}

			// If we have filled up our vector, we will just break out the loop.
			else
			{
				break;
			}
		}
	}

	// Return the vector with the filtered items
	return FilteredFoodVector;
}


// Compute the optimal set of food items with a greedy algorithm.
// Specifically, among the food items that fit within a total_weight,
// choose the foods whose calories-per-weight is greatest.
// Repeat until no more food items can be chosen, either because we've
// run out of food items, or run out of space.
std::unique_ptr<FoodVector> greedy_max_calories
(
	const FoodVector& foods,
	double total_weight
)
{
	// This is the structure for making the vector that has the cal/weight and the item
	struct percentItem
	{
		double percent;
		FoodItem item;

		percentItem(double p, FoodItem n) : percent(p), item(n) {}

	};

	// This is a comparator that will sort the percentItemVector based off the
	// cal/weight from high to low.
	struct sortPercentage
	{
		inline bool operator() (const percentItem firstSet, const percentItem secondSet)
		{
			return (firstSet.percent > secondSet.percent);
		}
	};

	std::vector <percentItem> percentItemVector;
	std::unique_ptr<FoodVector> GreedyFoodVector(new FoodVector);

	// Initialize the current capacity, the temporary weight of the item with the
	// capacity, and the cal/weight.
	double capacity = 0;
	double tempWeight = 0;
	double percentage = 0;

	// This for loop will add the percent of cal/weight with the item to a new vector
	for(auto& food: foods)
	{
			percentage = (food->foodCalories())/(food->weight());
			percentItemVector.push_back(percentItem(percentage, *food));
	}

	// We will sort based off the percent of cal/weight for each item
	std::sort(percentItemVector.begin(), percentItemVector.end(), sortPercentage());

	// This for loop will do the greedy algorithm
	for(int i = 0; i < int(percentItemVector.size()); i++)
	{
		// We find the temporary weight by adding the capacity with the item we are on.
		// The if statement will make sure we are still within the weight limit.
		tempWeight = capacity + percentItemVector[i].item.weight();
		if (tempWeight <= total_weight)
		{
			// If we are within the weight limit, we will update the capacity and add
			// the food item.
			capacity += percentItemVector[i].item.weight();
			GreedyFoodVector->push_back(std::shared_ptr<FoodItem>(new FoodItem(percentItemVector[i].item)));
		}
	}

	// Return the vector with the items that satisfy the algorithm
	return GreedyFoodVector;
}


// Compute the optimal set of food items with a exhaustive search algorithm.
// Specifically, among all subsets of food items, return the subset
// whose weight in ounces fits within the total_weight one can carry and
// whose total calories is greatest.
// To avoid overflow, the size of the food items vector must be less than 64.
std::unique_ptr<FoodVector> exhaustive_max_calories
(
	const FoodVector& foods,
	double total_weight
)
{
	// Initialize the Vectors that we will be using. The BestFoodVector will have
	// the best combination and will be what is returned. The CandidateFoodVector
	// will be what we use to keep checking what the best combination is.
	std::unique_ptr<FoodVector> BestFoodVector(new FoodVector);
	auto CandidateFoodVector(foods);

	// We will Initialize what we need for our loop and the weights and calories.
	int bitSize = pow(2, foods.size());
	double bestTotalCalries = 0;
	double candTotalWeight = 0;
	double candTotalCalories = 0;

	for (int bit = 0; bit < bitSize; bit++)
	{
		// We will keep clearing this vector to get ready for the next candidate.
		// We also set the candidate weight and calorie back to 0.
		CandidateFoodVector.clear();
		candTotalWeight = 0;
		candTotalCalories = 0;
		for (int j = 0; j < int(foods.size()); j++)
		{
			if (((bit >> j) & 1) == 1)
			{
				// Adding elements to our candidate vector.
				CandidateFoodVector.push_back(foods[j]);
				candTotalWeight += foods[j]->weight();
				candTotalCalories += foods[j]->foodCalories();
			}
			if (candTotalWeight <= total_weight)
			{
				if (candTotalCalories > bestTotalCalries)
				{
					// We will clear the best veector to get ready to update the best.
					BestFoodVector->clear();
					bestTotalCalries = candTotalCalories;
					// Adding the elements from the candidate to the best vector.
					for (auto& food : CandidateFoodVector)
					{
						BestFoodVector->push_back(food);
					}
				}
			}
		}
	}
	// Return the vector with the items that satisfy the algorithm
	return BestFoodVector;
}
