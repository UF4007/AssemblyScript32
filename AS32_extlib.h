inline static bool prints(func_t* caller)
{
    int32_t* virtual_char_ptr;
    if (caller->addressing_esp(virtual_char_ptr) == false)
        return false;
    int32_t* literal_char_ptr;
    if (caller->addressing_w(oprand_t::IA, *virtual_char_ptr, literal_char_ptr) == false)
        return false;
    std::printf((char*)literal_char_ptr);
    return true;
}
inline static bool printd(func_t* caller)
{
    int32_t* virtual_int_ptr;
    if (caller->addressing_esp(virtual_int_ptr) == false)
        return false;
    std::cout << *virtual_int_ptr;
    return true;
}
inline static asc::extfunc_t extlib[] = { 
    asc::prints,
    asc::printd };