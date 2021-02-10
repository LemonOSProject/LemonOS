#include <lemon/core/url.h>

#include <lemon/core/lexer.h>

namespace Lemon{
	URL::URL(const char* url){
		enum ParseState{
			Scheme,
			Authority,
			Port,
			Path,
			Query,
		};

		BasicLexer lex(url);
		ParseState pState = Scheme;
		
		while(!lex.End()){
			if(pState == Scheme){
				std::string_view scheme = lex.EatWhile(isalnum);

				if(lex.End()){
					host = scheme;
					break;
				}
				
				char c = lex.Eat();
				switch (c)
				{
				case ':':
					if(lex.Peek() == '/'){
						protocol = scheme;

						lex.EatOne('/');
						lex.EatOne('/'); // Consume up to two slashes

						break; // If the next character is a slash then this is a scheme
						// otherwise scheme probably does not exist and it is a port
					}
				case '@':
				case '.':
				case '/':
					pState = Authority;

					lex.Restart();
					continue;
				default:
					break;
				}

				pState = Authority;
			} else if(pState == Authority){
				std::string_view auth = lex.EatWhile([](int c) -> int {
					return isalnum(c) || c == '.';
				});

				if(lex.End()){
					host = auth;
					break;
				}

				char c = lex.Eat();
				switch(c){
				case '@':
					userinfo = auth;
					break;
				case ':':
					host = auth;

					pState = Port;
					break;
				case '/':
					host = auth;

					pState = Path;
					break;
				default:
					return; // Unknown character
				}
			} else if(pState == Port){
				std::string_view auth = lex.EatWhile(isdigit);

				if(lex.End()){
					port = auth;
					break;
				}

				char c = lex.Eat();
				switch(c){
				case '/':
					pState = Path;

					port = auth;
					break;
				default:
					return; // Unknown character
				}
			} else if(pState == Path){
				resource = lex.EatWhile([](int) -> int { return true; });
				break;
			}
		}

		valid = true;
	}
}