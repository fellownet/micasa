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
		if ( now - timestamp_ < 5 ) {
			return 'now';
		} else if ( now - timestamp_ < 60 ) {
			return Math.floor( now - timestamp_ ) + ' seconds';
		} else if ( now - timestamp_ < 60 * 60 ) {
			return Math.floor( ( now - timestamp_ ) / 60 ) + ' minutes';
		} else {
			let date: Date = new Date( timestamp_ * 1000 );
			return date.toLocaleDateString() + ' ' + date.toLocaleTimeString( [], { hour: '2-digit', minute: '2-digit' } );
		}
	}
}
