! Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
!
! Licensed under the Apache License, Version 2.0 (the "License");
! you may not use this file except in compliance with the License.
! You may obtain a copy of the License at
!
!     http://www.apache.org/licenses/LICENSE-2.0
!
! Unless required by applicable law or agreed to in writing, software
! distributed under the License is distributed on an "AS IS" BASIS,
! WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
! See the License for the specific language governing permissions and
! limitations under the License.

! See Fortran 2018, clause 18.2

module iso_c_binding

  type :: c_ptr
    integer(kind=8) :: address
  end type c_ptr

  type :: c_funptr
    integer(kind=8) :: address
  end type c_funptr

  type(c_ptr), parameter :: c_null_ptr = c_ptr(0)
  type(c_funptr), parameter :: c_null_funptr = c_funptr(0)

  ! Table 18.2 (in clause 18.3.1)
  ! TODO: Specialize (via macros?) for alternative targets
  integer, parameter :: &
    c_int8_t = 1, &
    c_int16_t = 2, &
    c_int32_t = 4, &
    c_int64_t = 8, &
    c_int128_t = 16 ! anticipating future addition
  integer, parameter :: &
    c_int = c_int32_t, &
    c_short = c_int16_t, &
    c_long = c_int64_t, &
    c_long_long = c_int64_t, &
    c_signed_char = c_int8_t, &
    c_size_t = c_long_long, &
    c_intmax_t = c_int128_t, &
    c_intptr_t = c_size_t, &
    c_ptrdiff_t = c_size_t
  integer, parameter :: &
    c_int_least8_t = c_int8_t, &
    c_int_fast8_t = c_int8_t, &
    c_int_least16_t = c_int16_t, &
    c_int_fast16_t = c_int16_t, &
    c_int_least32_t = c_int32_t, &
    c_int_fast32_t = c_int32_t, &
    c_int_least64_t = c_int64_t, &
    c_int_fast64_t = c_int64_t, &
    c_int_least128_t = c_int128_t, &
    c_int_fast128_t = c_int128_t

  integer, parameter :: &
    c_float = 4, &
    c_double = 8, &
#if __x86_64__
    c_long_double = 10
#else
    c_long_double = 16
#endif

  integer, parameter :: &
    c_float_complex = c_float, &
    c_double_complex = c_double, &
    c_long_double_complex = c_long_double

  integer, parameter :: c_bool = 1 ! TODO: or default LOGICAL?
  integer, parameter :: c_char = 1

 contains

  logical function c_associated(c_ptr_1, c_ptr_2)
    type(c_ptr), intent(in) :: c_ptr_1
    type(c_ptr), intent(in), optional :: c_ptr_2
    if (c_ptr_1%address == c_null_ptr%address) then
      c_associated = .false.
    else if (present(c_ptr_2)) then
      c_associated = c_ptr_1%address == c_ptr_2%address
    else
      c_associated = .true.
    end if
  end function c_associated

  subroutine c_f_pointer(cptr, fptr, shape)
    type(c_ptr), intent(in) :: cptr
    type(*), pointer, dimension(..), intent(out) :: fptr
    ! TODO: Use a larger kind for shape than default integer
    integer, intent(in), optional :: shape(:) ! size(shape) == rank(fptr)
    ! TODO: Define, or write in C and change this to an interface
  end subroutine c_f_pointer

  function c_loc(x)
    type(c_ptr) :: c_loc
    type(*), intent(in) :: x
    c_loc = c_ptr(loc(x))
  end function c_loc

  function c_funloc(x)
    type(c_funptr) :: c_funloc
    external :: x
    c_funloc = c_funptr(loc(x))
  end function c_funloc

  ! TODO c_f_procpointer
  ! TODO c_sizeof

end module iso_c_binding
