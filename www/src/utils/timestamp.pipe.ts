import {
	Pipe,
	PipeTransform
}                 from '@angular/core';

@Pipe( {
	name: 'timestamp'
} )

export class TimestampPipe implements PipeTransform {

	public transform( timestamp_: number ): string {
		let now: number = Math.floor( Date.now() / 1000 );
		let age: number = Math.abs( timestamp_ );
		if ( timestamp_ > 0 ) {
			age = now - timestamp_;
		}
		if ( age < 5 ) {
			return 'now';
		} else if ( age < 60 ) {
			return Math.floor( age ) + ' seconds';
		} else if ( age < 60 * 60 ) {
			return Math.floor( ( age ) / 60 ) + ' minutes';
		} else {
			let date: Date = new Date( ( now - age ) * 1000 );
			return date.toLocaleDateString() + ' ' + date.toLocaleTimeString( [], { hour: '2-digit', minute: '2-digit' } );
		}
	}
}
