/*
 * Copyright (c) 2020, Matthew Olsson <mattco@serenityos.org>
 * Copyright (c) 2023, Preston Taylor <95388976+PrestonLTaylor@users.noreply.github.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/ExceptionOrUtils.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/Bindings/SVGElementPrototype.h>
#include <LibWeb/CSS/ComputedProperties.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/ShadowRoot.h>
#include <LibWeb/SVG/SVGElement.h>
#include <LibWeb/SVG/SVGSVGElement.h>
#include <LibWeb/SVG/SVGUseElement.h>

namespace Web::SVG {

SVGElement::SVGElement(DOM::Document& document, DOM::QualifiedName qualified_name)
    : Element(document, move(qualified_name))
{
}

void SVGElement::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(SVGElement);
}

void SVGElement::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    HTMLOrSVGElement::visit_edges(visitor);
    visitor.visit(m_class_name_animated_string);
}

void SVGElement::attribute_changed(FlyString const& local_name, Optional<String> const& old_value, Optional<String> const& value, Optional<FlyString> const& namespace_)
{
    Base::attribute_changed(local_name, old_value, value, namespace_);
    HTMLOrSVGElement::attribute_changed(local_name, old_value, value, namespace_);

    update_use_elements_that_reference_this();
}

WebIDL::ExceptionOr<void> SVGElement::cloned(DOM::Node& copy, bool clone_children)
{
    TRY(Base::cloned(copy, clone_children));
    TRY(HTMLOrSVGElement::cloned(copy, clone_children));
    return {};
}

void SVGElement::inserted()
{
    Base::inserted();
    HTMLOrSVGElement::inserted();

    update_use_elements_that_reference_this();
}

void SVGElement::children_changed()
{
    Base::children_changed();

    update_use_elements_that_reference_this();
}

void SVGElement::update_use_elements_that_reference_this()
{
    if (is<SVGUseElement>(this)
        // If this element is in a shadow root, it already represents a clone and is not itself referenced.
        || is<DOM::ShadowRoot>(this->root())
        // If this does not have an id it cannot be referenced, no point in searching the entire DOM tree.
        || !id().has_value()
        // An unconnected node cannot have valid references.
        // This also prevents searches for elements that are in the process of being constructed - as clones.
        || !this->is_connected()
        // Each use element already listens for the completely_loaded event and then clones its referece,
        // we do not have to also clone it in the process of initial DOM building.
        || !document().is_completely_loaded()) {

        return;
    }

    document().for_each_in_subtree_of_type<SVGUseElement>([this](SVGUseElement& use_element) {
        use_element.svg_element_changed(*this);
        return TraversalDecision::Continue;
    });
}

void SVGElement::removed_from(Node* parent)
{
    Base::removed_from(parent);

    remove_from_use_element_that_reference_this();
}

void SVGElement::remove_from_use_element_that_reference_this()
{
    if (is<SVGUseElement>(this) || !id().has_value()) {
        return;
    }

    document().for_each_in_subtree_of_type<SVGUseElement>([this](SVGUseElement& use_element) {
        use_element.svg_element_removed(*this);
        return TraversalDecision::Continue;
    });
}

// https://svgwg.org/svg2-draft/types.html#__svg__SVGElement__classNames
GC::Ref<SVGAnimatedString> SVGElement::class_name()
{
    // The className IDL attribute reflects the ‘class’ attribute.
    if (!m_class_name_animated_string)
        m_class_name_animated_string = SVGAnimatedString::create(realm(), *this, AttributeNames::class_);

    return *m_class_name_animated_string;
}

// https://svgwg.org/svg2-draft/types.html#__svg__SVGElement__ownerSVGElement
GC::Ptr<SVGSVGElement> SVGElement::owner_svg_element()
{
    // The ownerSVGElement IDL attribute represents the nearest ancestor ‘svg’ element.
    // On getting ownerSVGElement, the nearest ancestor ‘svg’ element is returned;
    // if the current element is the outermost svg element, then null is returned.
    return shadow_including_first_ancestor_of_type<SVGSVGElement>();
}

GC::Ref<SVGAnimatedLength> SVGElement::svg_animated_length_for_property(CSS::PropertyID property) const
{
    // FIXME: Create a proper animated value when animations are supported.
    auto make_length = [&] {
        if (auto const computed_properties = this->computed_properties()) {
            if (auto length = computed_properties->length_percentage(property); length.has_value())
                return SVGLength::from_length_percentage(realm(), *length);
        }
        return SVGLength::create(realm(), 0, 0.0f);
    };
    return SVGAnimatedLength::create(realm(), make_length(), make_length());
}

}
