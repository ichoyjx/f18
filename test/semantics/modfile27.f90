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

! Test folding of combined array references and structure component
! references.

module m1
  type :: t1
    integer :: ia1(2)
  end type t1
  type(t1), parameter :: t1x1(:) = [t1::t1(ia1=[1,2]),t1(ia1=[3,4])]
  logical, parameter :: t1check1 = t1x1(2)%ia1(1) == 3
  logical, parameter :: t1check2 = all(t1x1(1)%ia1 == [1,2])
  logical, parameter :: t1check3 = all(t1x1(:)%ia1(1) == [1,3])
  type :: t2
    type(t1) :: dta1(2)
  end type t2
  type(t2), parameter :: t2x1(:) = &
    [t2 :: t2(dta1=[t1::t1x1]), &
           t2(dta1=[t1::t1(ia1=[5,6]),t1(ia1=[7,8])])]
  logical, parameter :: t2check1 = t2x1(1)%dta1(2)%ia1(2) == 4
  logical, parameter :: t2check2 = &
    all(t2x1(2)%dta1(2)%ia1(:) == [7,8])
  logical, parameter :: t2check3 = &
    all(t2x1(1)%dta1(:)%ia1(2) == [2,4])
  logical, parameter :: t2check4 = &
    all(t2x1(:)%dta1(1)%ia1(2) == [2,6])
end module m1
!Expect: m1.mod
!module m1
!type::t1
!integer(4)::ia1(1_8:2_8)
!end type
!type(t1),parameter::t1x1(1_8:)=[t1::t1(ia1=[Integer(4)::1_4,2_4]),t1(ia1=[Integer(4)::3_4,4_4])]
!logical(4),parameter::t1check1=.true._4
!logical(4),parameter::t1check2=.true._4
!logical(4),parameter::t1check3=.true._4
!type::t2
!type(t1)::dta1(1_8:2_8)
!end type
!type(t2),parameter::t2x1(1_8:)=[t2::t2(dta1=[t1::t1(ia1=[Integer(4)::1_4,2_4]),t1(ia1=[Integer(4)::3_4,4_4])]),t2(dta1=[t1::t1(ia1=[Integer(4)::5_4,6_4]),t1(ia1=[Integer(4)::7_4,8_4])])]
!logical(4),parameter::t2check1=.true._4
!logical(4),parameter::t2check2=.true._4
!logical(4),parameter::t2check3=.true._4
!logical(4),parameter::t2check4=.true._4
!end
