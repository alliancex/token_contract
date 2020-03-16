/*

    alliancex token contract. 

*/

#pragma once

#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using namespace std; 

namespace alliancex
{



    CONTRACT token: public contract {
    public:
        token( name receiver, name code, datastream<const char*> ds ) : contract(receiver, code, ds) {};

        ACTION create( const name& issuer, const asset& maximum_supply );
        ACTION issue ( const name& to, const asset& quantity, const string& memo );
        ACTION retire( const asset& quantity, const string& memo );
        ACTION transfer( const name& from, const name& to, const asset& quantity, const string& memo );
        ACTION open( const name& owner, const symbol& symbol, const name& ram_payer );
        ACTION close( const name& owner, const symbol& symbol );
        ACTION incms( const asset& quantity, const string& memo );
        ACTION decms( const asset& quantity, const string& memo );
        ACTION lockup( const name& account, const asset& quantity, const time_point& expire_time, const string& memo );

        //ACTION planunlock( const name& account, const asset& quantity, const time_point& time, const uint32_t& span_hour, const string& memo );
        //ACTION claimunlock( const name& account, const symbol& symbol );

        static asset get_supply( const name& token_contract_account, const symbol_code& sym_code )
        {
            stats_t statstable( token_contract_account, sym_code.raw() );
            const auto& st = statstable.get( sym_code.raw() );
            return st.supply;
        }

        static asset get_balance( const name& token_contract_account, const name& owner, const symbol_code& sym_code )
        {
            accounts_t accountstable( token_contract_account, owner.value );
            const auto& ac = accountstable.get( sym_code.raw() );
            return ac.balance;
        }
    private:
        // scope: account
        TABLE account {
            asset       balance;

            uint64_t primary_key() const { return balance.symbol.code().raw(); }
        };
        typedef eosio::multi_index< "accounts"_n, account > accounts_t;


        // scope: code
        TABLE currency_stats {
            asset       supply;
            asset       max_supply;
            name	    issuer;

            uint64_t primary_key() const { return supply.symbol.code().raw(); }
        };
        typedef eosio::multi_index< "stat"_n, currency_stats > stats_t;

        // scope: account
        TABLE lockup_info {
            asset       balance;
            time_point  expire_time;

            uint64_t primary_key() const { return balance.symbol.code().raw(); }
        };
        typedef eosio::multi_index< "lockups"_n, lockup_info > lockups_t;

        /*
        // scope: account
        // Lock free plan info
        TABLE plan4unlock_info {
            asset       quantity;
            time_point  time;
            uint32_t    span_hour;

            uint64_t primary_key() const { return quantity.symbol.code().raw(); }
        };
        typedef eosio::multi_index< "plan4unlock"_n, plan4unlock_info > plan4unlock_t;
        */

    private:
        bool check_lockup( const name& owner, const asset& value );

        void sub_balance( const name& owner, const asset& value );
        void add_balance( const name& owner, const asset& value, const name& ram_payer );
    };
    


} // name space of alliancex
