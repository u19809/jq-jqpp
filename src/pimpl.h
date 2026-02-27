#pragma once

#include <type_traits>
#include <memory>

template< typename PrivateCls>
struct HasPrivate {

    // only if PrivateCls is subclass of PrivateCls
    template< typename SubPrivateCls,
              typename = typename std::enable_if< std::is_base_of< PrivateCls, SubPrivateCls >::value >::type >
    HasPrivate( SubPrivateCls * P ) : ptr( 0 ) {
        ptr = std::shared_ptr<PrivateCls>( P );
    }

    virtual ~ HasPrivate( ) {
    }

    inline bool hasPrv() const {
        return ptr.get() != nullptr;
    }

    template< typename SubPrivateCls >
    inline SubPrivateCls & prv(){
        return *(dynamic_cast<SubPrivateCls *>(ptr.get()));
    }

    template< typename SubPrivateCls >
    inline const SubPrivateCls & prv() const {
        return *(dynamic_cast<SubPrivateCls *>(ptr.get()));
    }

protected :

    std::shared_ptr<PrivateCls> ptr;
};

template< typename PublicCls>
struct HasPublic  {

    HasPublic( PublicCls * X ) : ptr( 0 ) {
        ptr = X;
    }

    virtual ~ HasPublic( ) {
    }

    inline bool hasPub() const {
        return ptr != nullptr;
    }

    template< typename SubPublicCls >
    inline SubPublicCls & pub(){
        return *(dynamic_cast<SubPublicCls *>(ptr));
    }

    template< typename SubPublicCls >
    inline const SubPublicCls & pub() const {
        return *(dynamic_cast<SubPublicCls *>(ptr));
    }

protected :

    PublicCls * ptr;
};

/*
 *
 * USAGE
 *
 * REMARK : private pointer will be deleted automatically
 * REMARK : private pointer is shared so it can be copy constructed without explicit copy constructor
 *
 * For complete example, seem PimplTest test program
 *
 * Say we have a class A and B where B inherits from A and both want Pimpl
 *
 * in A.h
 * ------
 *
 * class A_Private;
 *
 * class A  : private HasPrivate<A_Private> {
 *
 *   public :
 *
 *     A(); // see implementation
 *     A(...); // see implementation
 *     A( A_Private * C );
 *     ~A() {
 *     prv<A_Private>().delete();
 *
 *     void aPublicMethod();
 *
 * };
 *
 * in A.cpp
 * --------
 *
 * #include <A.h>
 *
 * struct A_Private : public HasPublic<A> {
 *
 * 	A_Private( A * p ) : HasPublic<A>( p );
 *  ~A_Private() {
 *    // cleanup fields
 *  }
 *
 * 	void aPrivateMethod() {
 *      // access features of public class A
 *      pub<A>().aPublicMethod();
 *  }
 *
 * 	int aPrivateData;
 *
 * };
 *
 * A::A( A_Private * p ) : HasPrivate<A_Private>( this ) {
 * {
 *   // init default field values here
 *   prv<A_Private>().aPrivateData = 111;
 * }
 *
 * A::A() : A( new A_Private( this ) ) {
 * }
 *
 * A::A(...) : A( new A_Private( this ) ) {
 *    // ... non default initialization
 * }
 *
 * REMARK : destruction of private pointer is automatic upon destruction
 * of public part
 *
 * A::aPublicMethod() {
 * {
 *   return prv<A_Private>().aPrivateData;
 * }
 *
 * in B.h
 * ------
 *
 * #include <A.h>
 * class B_Private;
 *
 * class B : public A {
 *
 *   public :
 *
 *     B(); // see implementation
 *     B(...); // see implementation
 *     B( B_Private * C );
 *     ~B();
 *
 *     void bPublicMethod();
 *
 * };
 *
 * in B.cpp
 * --------
 *
 * #include <B.h>
 *
 * struct B_Private : public A_Private {
 *
 * 	B_Private( B * p ) : A_Private( p );
 *  ~B_Private() {
 *    // cleanup B fields
 *  }
 *
 * 	void bPrivateMethod() {
 *      // access features of public class B
 *      pub<B>().bPublicMethod();
 *      // or a
 *      pub<B>().aPublicMethod();
 *      // or
 *      pub<A>().aPublicMethod();
 *      // but not
 *      pub<A>().bPublicMethod();
 *
 *  }
 *
 * 	int bPrivateData;
 *
 * };
 *
 * B::B( B_Private * p ) : A( p ) {
 * {
 *   // init default field values here
 *   prv<B_Private>().bPrivateData = 111;
 * }
 *
 * B::B() : A( new B_Private( this ) ) {
 * }
 *
 * B::B(...) : B() {
 *    // ... non default initialization
 * }
 *
 * B::bPublicMethod() {
 * {
 *   return prv<B_Private>().bPrivateData;
 * }
 *
 */

