#include <gtest/gtest.h>
#include "OrderBook.h"

using namespace market_handler;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;

    void SetUp() override {
    }
};

TEST_F(OrderBookTest, InitialStateIsEmpty) {
    EXPECT_EQ(book.get_best_bid(), 0);
    EXPECT_EQ(book.get_best_ask(), UINT32_MAX);
}

TEST_F(OrderBookTest, AddAndCancelUpdatesBBO) {
    book.add_order(1, 100500, 100, true);
    EXPECT_EQ(book.get_best_bid(), 100500);

    book.add_order(2, 100600, 100, false);
    EXPECT_EQ(book.get_best_ask(), 100600);

    book.cancel_order(1);
    EXPECT_EQ(book.get_best_bid(), 0);

    book.cancel_order(2);
    EXPECT_EQ(book.get_best_ask(), UINT32_MAX);
}

TEST_F(OrderBookTest, ExactCrossFillsCompletely) {
    book.add_order(1, 100500, 100, false);
    
    book.add_order(2, 100500, 100, true);  
    
    EXPECT_EQ(book.get_best_ask(), UINT32_MAX);
    EXPECT_EQ(book.get_best_bid(), 0);
}

TEST_F(OrderBookTest, PartialFillLeavesRestingOrder) {
    book.add_order(1, 100500, 1000, false); 
    
    book.add_order(2, 100500, 100, true);  
    
    EXPECT_EQ(book.get_best_ask(), 100500);
}

TEST_F(OrderBookTest, AggressiveOrderSweepsMultipleLevels) {
    book.add_order(1, 100500, 100, false);
    book.add_order(2, 100505, 100, false);
    
    EXPECT_EQ(book.get_best_ask(), 100500);

    book.add_order(3, 100510, 200, true); 

    EXPECT_EQ(book.get_best_ask(), UINT32_MAX);
}

TEST_F(OrderBookTest, ModifyDownMaintainsBBO) {
    book.add_order(1, 100500, 100, true);
    book.modify_order(1, 100500, 50);
    
    EXPECT_EQ(book.get_best_bid(), 100500);
}

TEST_F(OrderBookTest, ModifyPriceTriggersCancelAndReplace) {
    book.add_order(1, 100500, 100, true);
    
    book.modify_order(1, 100400, 100); 

    EXPECT_EQ(book.get_best_bid(), 100400);
}
