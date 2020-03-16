#include "alliancex.token.hpp"

namespace alliancex {

/*
time_point current_time_point() {
    const static time_point ct{ eosio::microseconds{ static_cast<int64_t>( current_time() ) } };
    return ct;
}
 */

void token::create( const name& issuer, const asset& maximum_supply ) {
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( maximum_supply.is_valid(), "invalid supply");
    check( maximum_supply.amount > 0, "max-supply must be positive");

    stats_t statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}

void token::issue ( const name& to, const asset& quantity, const string& memo ) {
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats_t statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must issue positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
        SEND_INLINE_ACTION( *this, transfer, { {st.issuer, "active"_n} },
                          { st.issuer, to, quantity, memo }
      );
    }
}

void token::retire( const asset& quantity, const string& memo ) {
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats_t statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}

void token::transfer( const name& from, const name& to, const asset& quantity, const string& memo ) {
    // check( false, "Token transfer locked" );

    check( from != to, "cannot transfer to self" );
    require_auth( from );
    check( is_account( to ), "to account does not exist");

    auto sym = quantity.symbol.code();
    stats_t statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    check( !check_lockup(from, quantity), "request quantity has locked");

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer ) {
    require_auth( ram_payer );

    auto sym_code_raw = symbol.code().raw();

    stats_t statstable( _self, sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
    check( st.supply.symbol == symbol, "symbol precision mismatch" );

    accounts_t acnts( _self, owner.value );
    auto it = acnts.find( sym_code_raw );
    if( it == acnts.end() ) {
        acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = asset{0, symbol};
        });
    }
}

void token::close( const name& owner, const symbol& symbol ) {
    require_auth( owner );
    accounts_t acnts( _self, owner.value );
    auto it = acnts.find( symbol.code().raw() );
    check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
    check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
    acnts.erase( it );
}

void token::incms( const asset& quantity, const string& memo ) {
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats_t statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must decms positive quantity" );

    //check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.max_supply += quantity;
    });

}

void token::decms( const asset& quantity, const string& memo ) {
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats_t statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must decms positive quantity" );

    //check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    check( st.supply.amount <= st.max_supply.amount - quantity.amount, "quantity exceeds available supply" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.max_supply -= quantity;
    });

}

void token::lockup( const name& account, const asset& quantity, const time_point& expire_time, const string& memo ) {
    check( is_account( account ), "account is not exist");

    // Only issuer takes this action
    auto sym_code_raw = quantity.symbol.code().raw();

    stats_t statstable( _self, sym_code_raw );
    const auto st = statstable.get( sym_code_raw, "symbol does not exist" );
    check( quantity.amount >= 0, "must transfer positive quantity" );
	check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
	check( memo.size() <= 256, "memo has more than 256 bytes" );

    require_auth( st.issuer );

    lockups_t lockuptable( _self, account.value );
    auto lockup_it = lockuptable.find( sym_code_raw );


    if ( expire_time < current_time_point() || quantity.amount == 0 ) {
        // erase
        if ( lockup_it != lockuptable.end() ) {
            lockuptable.erase( lockup_it );
        }

    } else {
        // add or modify
        if ( lockup_it == lockuptable.end() ) {
            lockuptable.emplace( st.issuer, [&]( auto& r ) {
                r.balance = quantity; 
                r.expire_time = expire_time; 
            });
        } else {
            lockuptable.modify( lockup_it, same_payer, [&]( auto& r ){
                r.balance = quantity; 
                r.expire_time = expire_time; 
            });
        }
    }

}

/*
ACTION token::planunlock( const name& account, const asset& quantity, const time_point& time, const uint32_t& span_hour, const string& memo ) {
    check( is_account( account ), "account is not exist");

    // Only issuer takes this action

    // Verify symbol 
    auto sym_code_raw = quantity.symbol.code().raw();
    stats_t statstable( _self, sym_code_raw );
    const auto st = statstable.get( sym_code_raw, "symbol does not exist" );
    check( quantity.amount > 0, "must transfer positive quantity" );
	check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    require_auth( st.issuer );

    plan4unlock_t plan4unlock_table( _self, account.value );
    auto p4ul_it = plan4unlock_table.find( sym_code_raw );

    if ( quantity.amount == 0 ) {
        // erase
        if ( p4ul_it != plan4unlock_table.end() ) {
            plan4unlock_table.erase( p4ul_it );
        }

    } else {
        // add or modify
        if ( p4ul_it == plan4unlock_table.end() ) {
            plan4unlock_table.emplace( st.issuer, [&]( auto& r ) {
                r.quantity  = quantity;
                r.time      = time;
                r.span_hour = span_hour;
            });

        } else {
            plan4unlock_table.modify( p4ul_it, same_payer, [&]( auto& r ){
                r.quantity  = quantity;
                r.time      = time;
                r.span_hour = span_hour;
            });
        }
    }

}


ACTION token::claimunlock( const name& account, const symbol& symbol ) {
    require_auth( account );


    plan4unlock_t plan4unlock_table( _self, account.value );
    auto p4ul_it = plan4unlock_table.find( symbol.code().raw() );
    check( p4ul_it != plan4unlock_table.end(), "Unlock plan does not exists" ); 
    check( p4ul_it->time < current_time_point(), "The time has not expired" );

    lockups_t lockuptable( _self, account.value );
    auto lockup_it = lockuptable.find( symbol.code().raw() );
    check( lockup_it != lockuptable.end(), "Lockup table does not exists" );

    int64_t new_lock_amount = lockup_it->balance.amount - p4ul_it->quantity.amount;

    if ( new_lock_amount < 0 ) new_lock_amount = 0; 

    if ( new_lock_amount > 0 ) {
        lockuptable.modify( lockup_it, same_payer, [&]( auto& r ){
            r.balance.amount = new_lock_amount; 
        });

        plan4unlock_table.modify( p4ul_it, same_payer, [&]( auto& r){
            r.time      = r.time + hours(r.span_hour);
            // for test
            //r.time      = r.time + minutes(r.span_hour);
        });
    } else {
        lockuptable.erase( lockup_it );
        plan4unlock_table.erase( p4ul_it );
    }
}
*/

bool token::check_lockup( const name& owner, const asset& value ) {
    bool ret = false; 

    lockups_t lockuptable( _self, owner.value );
    const auto lockup_it = lockuptable.find( value.symbol.code().raw() );
    if ( lockup_it != lockuptable.end() ) {
        if ( lockup_it->expire_time >= current_time_point() ) {
            // if ( lockup_it->balance.amount >= value.amount ) {
            //     ret = true; 
            // }

            accounts_t acnts( _self, owner.value );
            const auto account_it = acnts.get( value.symbol.code().raw(), "no balance object found" );

            if ( account_it.balance.amount - lockup_it->balance.amount < value.amount ) {
                ret = true; 
            }
        }
    }

    return ret; 
}

void token::sub_balance( const name& owner, const asset& value ) {
    accounts_t from_acnts( _self, owner.value );

    const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
    check( from.balance.amount >= value.amount, "overdrawn balance" );

    from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
    });
}

void token::add_balance( const name& owner, const asset& value, const name& ram_payer ) {
    accounts_t to_acnts( _self, owner.value );

    const auto& to = to_acnts.find( value.symbol.code().raw() );
    if( to == to_acnts.end() ) {
        to_acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = value;
        });
    } else {
        to_acnts.modify( to, same_payer, [&]( auto& a ) {
            a.balance += value;
        });
    }
}


} // name space of alliancex


EOSIO_DISPATCH( alliancex::token, 
    (create)    (issue)     (retire)
    (transfer)  (open)      (close)
    (incms) (decms)

    (lockup)    
    
    //(planunlock)   (claimunlock)

)