! Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

integer, parameter :: k = 8
real, parameter :: l = 8.0
integer :: n = 2
!ERROR: Must be a constant value
parameter(m=n)
integer(k) :: x
!ERROR: Must have INTEGER type, but is REAL(4)
integer(l) :: y
!ERROR: Must be a constant value
integer(n) :: z
type t(k)
  integer, kind :: k
end type
!ERROR: Type parameter 'k' lacks a value and has no default
type(t( &
!ERROR: Must have INTEGER type, but is LOGICAL(4)
  .true.)) :: w
!ERROR: Must have INTEGER type, but is REAL(4)
real :: u(l*2)
!ERROR: Must have INTEGER type, but is REAL(4)
character(len=l) :: v
!ERROR: Initialization expression for PARAMETER 'o' (o) cannot be computed as a constant value
real, parameter ::  o = o
!ERROR: Must be a constant value
integer, parameter ::  p = 0/0
!ERROR: Must be a constant value
integer, parameter ::  q = 1+2*(1/0)
!ERROR: Must be a constant value
integer(kind=2/0) r
integer, parameter :: sok(:)=[1,2]/[1,2]
!ERROR: Must be a constant value
integer, parameter :: snok(:)=[1,2]/[1,0]
end
