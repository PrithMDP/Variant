#include <iostream>
#include <type_traits>
#include <utility>
#include <variant> // for std::bad_variant_access

using std::cout;
using std::endl;


// helper functions in details
namespace details {
    
    template <typename T>
    struct valid_parameter
    {
        static_assert(!std::is_array<T>::value, "Parameter cannot be an array");
        static_assert(!std::is_reference<T>::value, "Parameter cannot be a reference");
        static_assert(!std::is_same<T,void>::value, "Parameter cannot be void");
    };
    
    template <typename... Args>
    struct max_size_in_var_args {};

    template <typename T>
    struct max_size_in_var_args<T>
    {
        valid_parameter<T> obj {};
        static const int size = sizeof(T);
    };

    template <typename T, typename... Args>
    struct max_size_in_var_args<T, Args...>
    {
        valid_parameter<T> obj {};
        static const int size = sizeof(T);
        static const int size_rest = max_size_in_var_args<Args...>::size;
        static const int max_size = size > size_rest ? size : size_rest;
    };


    template <int N, typename... Args>
    struct type_list_contains {};

    template <int N, typename InBound>
    struct type_list_contains<N,InBound>
    {
        static const int in_list = -1;
    };

    template <int N, typename InBound, typename T, typename... Args>
    struct type_list_contains<N,InBound,T,Args...>
    {
        static const int in_list = std::is_same<T,InBound>::value ? N : type_list_contains<N+1,InBound,Args...>::in_list;
    };
    
    
    template <int start_size,typename In, typename... Args>
    struct type_index_checker
    {
        static const int index_of_type = -1;
    };
    
    template <int start_size, typename In, typename T, typename... Args>
    struct type_index_checker<start_size,In,T,Args...>
    {
        static const int index_of_type = std::is_same<In,T>::value ? (start_size - sizeof...(Args) -1) : type_index_checker<start_size,In,Args...>::index_of_type;
    };
    
    template <int Idx, int StartSize, typename... Args>
    struct indexed_type
    {
        using template_type = void; 
    };

    template <int Idx, int StartSize, typename T, typename... Args>
    struct indexed_type<Idx,StartSize,T,Args...>
    {
        static constexpr bool is_correct_idx = ((Idx + 1) == int(StartSize - int(sizeof...(Args))));
        using template_type = typename std::conditional<is_correct_idx,T,typename indexed_type<Idx,StartSize,Args...>::template_type>::type;
    };
    
}


template <typename... Args>
struct Var
{
    template <typename T, typename... VArgs, typename std::enable_if<!bool(sizeof...(VArgs))>::type* = nullptr>
    void index_to_type(int start_size, int idx)
    {
        (void)start_size;
        (void)idx;
        T* ptr = reinterpret_cast<T*>(&data);
        ptr->~T();
        return;
    }

    template <typename T, typename... VArgs, typename std::enable_if<bool(sizeof...(VArgs))>::type* = nullptr>
    void index_to_type(int start_size, int idx)
    {

        if((idx + 1) == int(start_size - int(sizeof...(VArgs))))
        {
            T* ptr = reinterpret_cast<T*>(&data);
            ptr->~T();
            return;
        }
        index_to_type<VArgs...>(start_size, idx);
    }

    template <typename... VArgs>
    void index_to_type_wrapper(int idx)
    {
        index_to_type<VArgs...>(sizeof...(VArgs),idx);
    }
    
    bool contains = false;
    int curr_idx;
    
    static constexpr int num_tmps = sizeof...(Args);
    static constexpr int max_size = details::max_size_in_var_args<Args...>::max_size;
 
    typename std::aligned_storage<max_size, alignof(std::max_align_t)>::type data;
    
    //default ctor
    // should probably do something similar to the std::monostate constraint
    // but not yet
    Var()
    {}
    
    // copy constructor and assignment operator for other Variants
    // copy constructor of variant with different Args should not compile
    template <typename... Vargs>
    Var(const Var<Vargs...>& other)
    {
        static_assert(std::is_same_v<decltype(std::declval<Var<Args...>>),decltype(std::declval<Var<Vargs...>>)>,"Variant type mismatch in copy ctor");
        
        //memcpy the bytes and set the index
        curr_idx = other.curr_idx;
        memcpy(&(data),&(other.data),max_size);
        
    }
    
    template <typename... Vargs>
    void operator=(const Var<Vargs...>& other)
    {
        static_assert(std::is_same_v<decltype(std::declval<Var<Args...>>),decltype(std::declval<Var<Vargs...>>)>,"Variant type mismatch in copy ctor");
        curr_idx = other.curr_idx;
        memcpy(&(data),&(other.data),max_size);
    }
    
    // Generic copy constructor and assignment operator
    
    template <typename T>
    Var(const T& obj) // change to const T&? 
    {
        curr_idx = details::type_list_contains<0,T,Args...>::in_list;
        if(curr_idx == -1)
        {
            throw std::bad_variant_access();
        }
        new(&data) T(obj);
        contains = true;
    }
    
    template <typename T>
    void operator=(const T& obj)
    {
        int old_idx = curr_idx;
        curr_idx = details::type_list_contains<0,T,Args...>::in_list;
        if( curr_idx == -1)
        {
            throw std::bad_variant_access();
        }
        if(contains)
        {
            index_to_type_wrapper<Args...>(old_idx);
        }
        else
        {
             contains = true;
        }
        new(&data) T(obj); 
    }

    // call the appropriate destructor if the variant was ever set
    ~Var()
    {
        if(contains)
        {
            index_to_type_wrapper<Args...>(curr_idx);
        }
    }
    
}; // variant

// Test class to make sure construction / destruction works
class test_class
{
    public:
    test_class()
    {
        cout << "test class ctor\n";
    }
    test_class(const test_class& obj)
    {
        (void) obj; // remove unused warning
        cout << "test class copy ctor\n";
    }
    ~test_class()
    {
        cout << "test class dtor\n";
    }
};

// Implementation of std::variant::get<INT>();
template <int N, typename... Args>
auto Var_get(const Var<Args...>& v) -> typename std::add_lvalue_reference<typename details::indexed_type<N,sizeof...(Args),Args...>::template_type>::type
{
    if(v.curr_idx != N)
    {
        throw std::bad_variant_access();
    }
    //replace with using.. eventually
    typedef typename details::indexed_type<N,sizeof...(Args),Args...>::template_type ReturnType; // return type
    
    ReturnType* ptr;
    
    ptr = const_cast<ReturnType*>(reinterpret_cast<const ReturnType*>((&v.data)));
    
    return *ptr;
    
}

// Implementation of std::variant::get<T>();
// Check if current type is actually T
template <typename T, typename... Args>
T& Var_get(const Var<Args...>& v)
{
    int index_of_T = details::type_index_checker<sizeof...(Args),T,Args...>::index_of_type;
    if (index_of_T != v.curr_idx)
    {
        throw std::bad_variant_access();
    }
    T *ptr = const_cast<T*>(reinterpret_cast<const T*>((&v.data)));
    return *ptr;
}


int main()
{
    Var<int,double,char> v{'c'};
    
    Var<int,test_class> vv = 1;
    
    test_class a;
    
    vv = a;

    Var_get<char>(v) = 'l';
    
    cout << Var_get<char>(v) << endl;
    
    
    Var<int,double,char> g;
    
    g = -981;
    
    v = g;
    
    cout << Var_get<int>(v) << endl;
        
    Var_get<0>(v) = 6;
    
    cout << Var_get<int>(v) << endl;
    
}
