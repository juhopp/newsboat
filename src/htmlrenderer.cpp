#include <htmlrenderer.h>
#include <xmlpullparser.h>
#include <utils.h>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <logger.h>
#include <libgen.h>
#include <config.h>

using namespace newsbeuter;

htmlrenderer::htmlrenderer(unsigned int width) : w(width) { }

void htmlrenderer::render(const std::string& source, std::vector<std::string>& lines, std::vector<linkpair>& links, const std::string& url) {
	std::istringstream input(source);
	render(input, lines, links, url);
}

std::string htmlrenderer::absolute_url(const std::string& url, const std::string& link) {
	if (link.substr(0,7)=="http://" || link.substr(0,8)=="https://" || link.substr(0,6)=="ftp://" || link.substr(0,7) == "mailto:"){
		return link;
	}
	char u[1024];
	snprintf(u, sizeof(u), "%s", url.c_str());
	if (link[0] == '/') {
		// this is probably the worst code in the whole program
		char * foo = strstr(u, "//");
		if (foo) {
			if (strlen(foo)>=2) {
				foo += 2;
				foo = strchr(foo,'/');
				char u2[1024];
				strcpy(u2, u);
				snprintf(u2 + (foo - u), sizeof(u2) - (foo - u), "%s", link.c_str());
				return u2;
			}
		}
		return link;
	} else {
		char * base = dirname(u);
		std::string retval(base);
		retval.append(1,'/');
		retval.append(link);
		return retval;
	}
}

unsigned int htmlrenderer::add_link(std::vector<linkpair>& links, const std::string& link, link_type type) {
	bool found = false;
	unsigned int i=0;
	for (std::vector<linkpair>::iterator it=links.begin();it!=links.end();++it, ++i) {
		if (it->first == link) {
			found = true;
			break;
		}
	}
	if (!found)
		links.push_back(linkpair(link,type));

	return i;
}

void htmlrenderer::render(std::istream& input, std::vector<std::string>& lines, std::vector<linkpair>& links, const std::string& url) {
	unsigned int image_count = 0;
	std::string curline;
	int indent_level = 0;
	bool inside_list = false, inside_li = false, is_ol = false, inside_pre = false;
	bool itunes_hack = false;
	unsigned int ol_count = 1;
	
	xmlpullparser xpp;
	xpp.setInput(input);
	
	for (xmlpullparser::event e = xpp.next(); e != xmlpullparser::END_DOCUMENT; e = xpp.next()) {	
		switch (e) {
			case xmlpullparser::START_TAG:
				GetLogger().log(LOG_DEBUG,"htmlrenderer::render: found start tag %s",xpp.getText().c_str());
				if (xpp.getText() == "a") {
					std::string link;
					try {
						link = xpp.getAttributeValue("href");
					} catch (const std::invalid_argument& ) {
						GetLogger().log(LOG_WARN,"htmlrenderer::render: found a tag with no href attribute");
						link = "";
					}
					if (link.length() > 0) {
						unsigned int link_num = add_link(links,absolute_url(url,link), LINK_HREF);
						std::ostringstream ref;
						ref << "[" << link_num << "]";
						curline.append(ref.str());
					}
				} else if (xpp.getText() == "embed") {
					std::string type;
					try {
						type = xpp.getAttributeValue("type");
					} catch (const std::invalid_argument& ) {
						GetLogger().log(LOG_WARN, "htmlrenderer::render: found embed object without type attribute");
						type = "";
					}
					if (type == "application/x-shockwave-flash") {
						std::string link;
						try {
							link = xpp.getAttributeValue("src");
						} catch (const std::invalid_argument& ) {
							GetLogger().log(LOG_WARN, "htmlrenderer::render: found embed object without src attribute");
							link = "";
						}
						if (link.length() > 0) {
							unsigned int link_num = add_link(links,absolute_url(url,link), LINK_EMBED);
							std::ostringstream ref;
							ref << "[" << _("embedded flash:") << " " << link_num  << "]";
							curline.append(ref.str());
						}
					}
				} else if (xpp.getText() == "br") {
						if (curline.length() > 0)
							lines.push_back(curline);
						prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "pre") {
					inside_pre = true;
					if (curline.length() > 0)
						lines.push_back(curline);
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "ituneshack") {
					itunes_hack = true;
				} else if (xpp.getText() == "img") {
					std::string imgurl;
					try {
						imgurl = xpp.getAttributeValue("src");
					} catch (const std::invalid_argument& ) {
						GetLogger().log(LOG_WARN,"htmlrenderer::render: found img tag with no src attribute");
						imgurl = "";
					}
					if (imgurl.length() > 0) {
						unsigned int link_num = add_link(links,absolute_url(url,imgurl), LINK_IMG);
						std::ostringstream ref;
						ref << "[" << _("image") << " " << link_num << "]";
						image_count++;
						curline.append(ref.str());
					}
				} else if (xpp.getText() == "blockquote") {
					++indent_level;
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back("");
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "p") {
					if (curline.length() > 0)
						lines.push_back(curline);
					if (lines.size() > 0 && lines[lines.size()-1].length() > static_cast<unsigned int>(indent_level*2))
						lines.push_back("");
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "ol") {
					inside_list = true;
					is_ol = true;
					ol_count = 1;
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back("");
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "ul") {
					inside_list = true;
					is_ol = false;
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back("");
					prepare_newline(curline, indent_level);
				} else if (xpp.getText() == "li") {
					if (inside_li) {
						indent_level-=2;
						if (curline.length() > 0)
							lines.push_back(curline);
						prepare_newline(curline, indent_level);	
					}
					inside_li = true;
					if (curline.length() > 0)
						lines.push_back(curline);
					prepare_newline(curline, indent_level);
					indent_level+=2;
					if (is_ol) {
						std::ostringstream num;
						num << ol_count;
						if (ol_count < 10)
							curline.append(" ");
						curline.append(num.str());
						curline.append(". ");
						++ol_count;
					} else {
						curline.append("  * ");
					}
				}
				break;
			case xmlpullparser::END_TAG:
				GetLogger().log(LOG_DEBUG, "htmlrenderer::render: found end tag %s",xpp.getText().c_str());
				if (xpp.getText() == "blockquote") {
					--indent_level;
					if (indent_level < 0)
						indent_level = 0;
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back("");
					prepare_newline(curline, indent_level);
				} else if (xpp.getText() == "ol" || xpp.getText() == "ul") {
					inside_list = false;
					if (inside_li) {
						indent_level-=2;
						if (curline.length() > 0)
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
					}
					if (curline.length() > 0)
						lines.push_back(curline);
					lines.push_back("");
					prepare_newline(curline, indent_level);	
				} else if (xpp.getText() == "li") {
					indent_level-=2;
					inside_li = false;
					if (curline.length() > 0)
						lines.push_back(curline);
					prepare_newline(curline, indent_level);
				} else if (xpp.getText() == "p") {
					if (curline.length() > 0)
						lines.push_back(curline);
					prepare_newline(curline, indent_level);
				} else if (xpp.getText() == "pre") {
					if (curline.length() > 0)
						lines.push_back(curline);
					prepare_newline(curline, indent_level);
					inside_pre = false;
				}
				break;
			case xmlpullparser::TEXT:
				{
					GetLogger().log(LOG_DEBUG,"htmlrenderer::render: found text `%s'",xpp.getText().c_str());
					if (itunes_hack) {
						std::vector<std::string> words = utils::tokenize_nl(xpp.getText());
						for (std::vector<std::string>::iterator it=words.begin();it!=words.end();++it) {
							if (*it == "\n") {
								lines.push_back(curline);
								prepare_newline(curline, indent_level);
							} else {
								std::vector<std::string> words = utils::tokenize_spaced(*it);
								unsigned int i=0;
								bool new_line = false;
								for (std::vector<std::string>::iterator it=words.begin();it!=words.end();++it,++i) {
									if ((curline.length() + it->length()) >= w) {
										if (curline.length() > 0)
											lines.push_back(curline);
										prepare_newline(curline, indent_level);
										new_line = true;
									}
									if (new_line) {
										if (*it != " ")
											curline.append(*it);
										new_line = false;
									} else {
										curline.append(*it);
									}
								}
							}
						}
					} else if (inside_pre) {
						std::vector<std::string> words = utils::tokenize_nl(xpp.getText());
						for (std::vector<std::string>::iterator it=words.begin();it!=words.end();++it) {
							if (*it == "\n") {
								lines.push_back(curline);
								prepare_newline(curline, indent_level);
							} else {
								curline.append(*it);
							}
						}
					} else {
						std::vector<std::string> words = utils::tokenize_spaced(xpp.getText());
						unsigned int i=0;
						bool new_line = false;
						for (std::vector<std::string>::iterator it=words.begin();it!=words.end();++it,++i) {
							if ((curline.length() + it->length()) >= w) {
								if (curline.length() > 0)
									lines.push_back(curline);
								prepare_newline(curline, indent_level);
								new_line = true;
							}
							if (new_line) {
								if (*it != " ")
									curline.append(*it);
								new_line = false;
							} else {
								curline.append(*it);
							}
						}
					}
				}
				break;
			default:
				/* do nothing */
				break;
		}
	}
	if (curline.length() > 0)
		lines.push_back(curline);
	
	if (links.size() > 0) {
		lines.push_back("");
		lines.push_back(_("Links: "));
		for (unsigned int i=0;i<links.size();++i) {
			std::ostringstream line;
			line << "[" << i << "]: " << links[i].first << " (" << type2str(links[i].second) << ")";
			lines.push_back(line.str());
		}
	}
}

std::string htmlrenderer::type2str(link_type type) {
	switch (type) {
		case LINK_HREF: return _("link");
		case LINK_IMG: return _("image");
		case LINK_EMBED: return _("embedded flash");
		default: return _("unknown (bug)");
	}
}

void htmlrenderer::prepare_newline(std::string& line, int indent_level) {
	line = "";
	for (int i=0;i<indent_level;++i) {
		line.append("  ");	
	}
}
