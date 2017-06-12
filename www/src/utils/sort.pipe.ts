import {
	Pipe,
	PipeTransform
}                 from '@angular/core';

@Pipe( {
	name: 'sort'
} )

export class SortPipe implements PipeTransform {

	public transform( data_: any[], field_: string, desc_: boolean = false ): any[] {
		if ( !! data_ ) {
			data_.sort( function( left_: any, right_: any ) {
				let a: any = ( typeof( left_[field_] ) == 'string' ? left_[field_].toUpperCase() : left_[field_] );
				let b: any = ( typeof( right_[field_] ) == 'string' ? right_[field_].toUpperCase() : right_[field_] );
				if ( a > b ) {
					return desc_ ? -1 : 1;
				}
				if ( a < b ) {
					return desc_ ? 1 : -1;
				}
				return 0;
			} );
			return data_;
		} else {
			return [];
		}
	}
}
